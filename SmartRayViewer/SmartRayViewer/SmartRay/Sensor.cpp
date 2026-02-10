#define NOMINMAX

#include "Sensor.h"
#include "SensorManager.h"

#include <sr_api_errorcodes.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>

//
// callback for data with not registered command numbers
//
int UnknownSensorCommandCallback( SRSensor* sensor )
{
	std::cout << "unknown command received for sensor: " << sensor->name << std::endl;
	return 0;
}

Sensor::Sensor( std::string name, int multiSensorIndex, const char* ipAddress, unsigned short port )
    : _pointCloudFilePrefix( "Point_Cloud_data" )
    , _saveAllAcquisitions( false )
    , _saveAllPoints( true )
    , _acquisitionTimeoutMs( 0 )
    , _acquisitionTimeoutStepMs( 0 )
    , _acquisitionCnt( 0 )
    , m_isCapturing( false )
{
	_sensor = new SRSensor();
	// sensor index (increment by one for each new sensor if you want to implement >1 sensors)
	_sensor->cam_index = multiSensorIndex;
	_sensor->active    = 0;
	// sensor name
	strcpy_s( _sensor->name, name.c_str() );

	// sensor IP address (can be changed ATTENTION!!!! Don't forget new IPAddress!)
	strcpy_s( _sensor->IPAdr, ipAddress );

	// sensor IP port number (can be changed ATTENTION!!!! Don't forget new IPAddress!)
	_sensor->portnum = port;

	// register data callback for "unknown" commands (advanced mode only)
	_sensor->usercbf = UnknownSensorCommandCallback;

	// init image & packet size information
	_numberOfExpectedProfiles = 0;
	_packetSize               = 0;
	_packetTimeout            = 0;

	// true when live images shall be acquired
	_liveImageMode = false;
	SetTransportResolution( DEFAULT_RESOLUTION );
	// true when MSR mode is enabled
	_msrMode        = false;
	_metaDataExport = false;
	_width          = 0;
}

Sensor::~Sensor()
{
	clearBuffer();
}

void Sensor::Connect()
{
	//=======================================
	// Connect to the sensor
	// Reconnect automatically if disconnected
	// Disconnect if connection timeout
	//=======================================
	std::cout << std::endl
	          << "Sensor: " << _sensor->name << " configured to " << _sensor->IPAdr << ":" << _sensor->portnum
	          << std::endl;

	int        timeoutS  = 60;
	static int err_cnt   = 0;
	static int err_cnt_t = 0;
	static int good_cnt  = 0;

	try
	{
		int ret = SR_API_ConnectSensor( _sensor.get(), timeoutS );
		if( ret )
		{
			std::cout << " Error while connecting (" << ret << "): " << ++err_cnt << std::endl;
		}
		else
		{
			std::cout << " Connect to sensor OK: " << ++good_cnt << std::endl;
		}
		SensorManager::HandleReturnCode( ret );
		GetSensorSeries();
	}
	catch( ... )
	{
		std::cout << "ERR: connect failed!: " << ++err_cnt_t << std::endl;
		// re-throw
		throw;
	}
}

void Sensor::GetSensorSeries()
{
	//=======================================
	// get sensor type & model name
	//=======================================
	char partNumber[16];
	char modelName[32];
	std::cout << "requesting sensor type..." << std::endl;
	int ret = SR_API_GetSensorModelName( _sensor.get(), modelName, partNumber );
	SensorManager::HandleReturnCode( ret );
	std::cout << "sensor model name: " << modelName << " part number: " << partNumber << std::endl;

	// extract sensor series from the sensor model name  which will be used to set parameter set path
	_sensorSeries.clear();
	for( unsigned int i = 0; i < strlen( modelName ); i++ )
	{
		if( modelName[i] == '.' )
			break;

		if( modelName[i] == ' ' )
			continue;

		if( _sensorSeries.size() == 4 && _sensorSeries.substr( 0, 2 ) == "SR" )
		{
			_sensorSeries += "00";
			break;
		}

		_sensorSeries += modelName[i];
	}
}

void Sensor::Disconnect()
{
	if( _sensorState.SensorConnection != SensorConnected )
	{
		return;
	}

	//=======================================
	// disconnect sensor & stop trying to reconnect
	//=======================================
	std::cout << "stop sensor connection" << std::endl;
	int ret = SR_API_DisconnectSensor( _sensor.get() );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::clearBuffer()
{
	ScopedLock guard( m_guard );
	clear();
}

void Sensor::clear()
{
	_images.clear();
	_meta_data.clear();
	_cbPointClouds.clear();
	_pointCloudsMSR.clear();
	_numberOfCapturedProfiles = 0;
}

template<typename T>
size_t safeErase( T& container, size_t count )
{
	count = std::min( container.size(), count );
	container.erase( container.begin(), container.begin() + count );
	return count;
}

void Sensor::clearReceivedImages( size_t count )
{
	ScopedLock guard( m_guard );
	safeErase( _images, count );
	safeErase( _meta_data, count );
}

const SensorState Sensor::GetSensorState()
{
	return _sensorState;
}

SharedPtr<SensorImageData> Sensor::GetLastImageData()
{
	ScopedLock guard( m_guard );
	// get last valid image
	for( size_t i = _images.size(); 0 < i; --i )
	{
		if( _images[i - 1]->ContainsFullImages() )
		{
			return _images[i - 1];
		}
	}
	return nullptr;
}

void Sensor::SavePointCloudsToFile( const uint32_t suffix )
{
	std::stringstream suffixStr;
	suffixStr << suffix;
	std::string fileName( _pointCloudFilePrefix + "_" + suffixStr.str() + ".txt" );
	PointCloud::PrintHeader( fileName, GetTransportResolution() );
	uint32_t lastPointIdx = 0;

	for( size_t i = 0; i < _cbPointClouds.size(); ++i )
	{  // Save points to file.
		lastPointIdx = _cbPointClouds[i].SavePointCloud( fileName, lastPointIdx, _saveAllPoints );
	}
}

SharedPtr<SensorPointcloudMSR> Sensor::GetLastPointcloudMSR()
{
	ScopedLock guard( m_guard );

	if( _pointCloudsMSR.empty() )
	{
		return nullptr;
	}
	SharedPtr<SensorPointcloudMSR> pointCloudMSR = _pointCloudsMSR.back();
	_pointCloudsMSR.pop_back();
	return pointCloudMSR;
}

SharedPtr<SensorImageData> Sensor::AddImageData( int           width,
                                                 int           height,
                                                 ImageDataType imageType,
                                                 int           originX,
                                                 float         originYMillimeters )
{
	ScopedLock guard( m_guard );

	if( _msrMode )
	{
		_images.push_back( new SensorImageData( width, height, imageType, originX, originYMillimeters, height ) );
	}
	else if( !_images.empty() && _images.back()->CurrentHeight < (int)_numberOfExpectedProfiles )
	{
		_images.back()->CurrentHeight += height;
		std::cout << "Sensor image updated, height: " << _images.back()->CurrentHeight << " ### ";
	}
	else
	{
		_images.push_back(
		    new SensorImageData( width, _numberOfExpectedProfiles, imageType, originX, originYMillimeters, height ) );
		std::cout << "New sensor image created, #images: " << _images.size() << " ### ";
	}

	return _images.back();
}

void Sensor::AddMetaData( int height, SR_MetaData* meta_data )
{
	ScopedLock guard( m_guard );

	_meta_data.push_back( new MetaData( make_vector( meta_data, height ) ) );
}

void Sensor::AddMSRPointCloudData( uint32_t    numPoints,
                                   SR_3DPOINT* point_cloud,
                                   uint16_t*   intensity,
                                   uint16_t*   laserlinethickness,
                                   uint32_t*   sensorIdx,
                                   uint32_t*   profileIdx,
                                   uint32_t*   pointIdx )
{
	ScopedLock guard( m_guard );

	_pointCloudsMSR.push_back( new SensorPointcloudMSR(
	    numPoints, point_cloud, intensity, laserlinethickness, sensorIdx, profileIdx, pointIdx ) );
	std::cout << "New MSR point cloud data: " << numPoints << " ### ";
}

void Sensor::AddPointCloudData( const uint32_t numPoints,
                                const uint32_t numProfile,
                                SR_3DPOINT*    point_cloud,
                                uint16_t*      intensity,
                                uint16_t*      laserlinethickness,
                                uint32_t*      profileIdx,
                                uint32_t*      columnIdx )
{
	while( _numberOfCapturedProfiles >= _numberOfExpectedProfiles )
	{
		// std::cout << "*** callback() waiting, processing of previous acqusition not complete." << std::endl;
		Sleep( 50 );
	}

	std::cout << "*** Point Cloud callback(): " << _sensor->name << std::endl;
	AddPointCloud( numPoints, numProfile, point_cloud, intensity, laserlinethickness, profileIdx, columnIdx );
	std::cout << "*** callback() finished." << std::endl;
}

void Sensor::AddPointCloud( const uint32_t numPoints,
                            const uint32_t numProfile,
                            SR_3DPOINT*    point_cloud,
                            uint16_t*      intensity,
                            uint16_t*      laserlinethickness,
                            uint32_t*      profileIdx,
                            uint32_t*      columnIdx )
{
	ScopedLock guard( m_guard );
	_cbPointClouds.push_back(
	    PointCloud( numPoints, point_cloud, intensity, laserlinethickness, profileIdx, columnIdx ) );
	_numberOfCapturedProfiles += numProfile;
}

void Sensor::ExportMetaData( std::string file_name )
{
	std::ofstream meta_data_file( file_name );
	if( !meta_data_file.good() )
	{
		return;
	}
	std::stringstream printable;

	printable << "Meta data export: " << '\n' << '\n';

	for( size_t i = 0; i < _meta_data.size(); ++i )
	{
		printable << " -------------------------- " << '\n';
		for( size_t j = 0; j < _meta_data[i]->size(); ++j )
		{
			printable << "Start trigger number  : " << ( *_meta_data[i] )[j].StartTriggerNb << '\n';
			printable << "Data trigger number   : " << ( *_meta_data[i] )[j].DataTriggerNb << '\n';
			printable << "Profile number        : " << ( *_meta_data[i] )[j].ProfileNb << '\n';
			printable << "Timestamp             : " << ( *_meta_data[i] )[j].TimeStamp << '\n';
			printable << "TimeStampSequence     : " << ( *_meta_data[i] )[j].TimeStampSequence << '\n';
			printable << "Input_0_State         : " << ( *_meta_data[i] )[j].Input_0_State << '\n';
			printable << "Input_1_State         : " << ( *_meta_data[i] )[j].Input_1_State << '\n';
			printable << "QuadStepCountFiltered : " << ( *_meta_data[i] )[j].QuadStepCountFiltered << '\n';
			printable << "QuadStepCountRaw      : " << ( *_meta_data[i] )[j].QuadStepCountRaw << '\n';
			printable << "TriggerOverflow       : " << ( *_meta_data[i] )[j].TriggerOverflow << '\n';
			printable << "OutputStatus          : " << ( *_meta_data[i] )[j].OutputStatus << '\n';
			printable << "DataTriggerOverflowCnt: " << ( *_meta_data[i] )[j].DataTriggerOverflowCnt << '\n';
			printable << std::endl;
		}

		printable << std::endl;
		_meta_data[i]->clear();
	}

	meta_data_file << printable.str();
	meta_data_file.close();


	_meta_data.clear();
}

void Sensor::StartAcquisition()
{
	int32_t ret = 0;

	if( !_liveImageMode )
	{
		// get the "number of profiles to capture" and "profile packet size"
		ret = SR_API_GetNumberOfProfilesToCapture( _sensor.get(), &_numberOfExpectedProfiles );
		SensorManager::HandleReturnCode( ret );
		ret = SR_API_GetPacketSize( _sensor.get(), &_packetSize );
		SensorManager::HandleReturnCode( ret );
		// ECCO 95 only, 500ms default
		SR_API_GetPacketTimeOut( _sensor.get(), &_packetTimeout );
	}
	else
	{
		// set number of profiles by the supplied ROI information in live mode
		int32_t originX, width, originY, height;
		ret = SR_API_GetROI( _sensor.get(), &originX, &width, &originY, &height );
		SensorManager::HandleReturnCode( ret );

		_numberOfExpectedProfiles = height;
		_width                    = width;
		_packetSize               = height;
		_packetTimeout            = 0;
	}

	// clear old image buffer
	clearBuffer();

	//=======================================
	// start the sensor
	//=======================================
	std::cout << "start sensor data acquisition." << std::endl;
	ret           = SR_API_StartAcquisition( _sensor.get() );
	m_isCapturing = ( SUCCESS == ret );
	SensorManager::HandleReturnCode( ret );

	std::cout << "Number of profiles to capture: " << _numberOfExpectedProfiles << std::endl;
	std::cout << "Profile packet size: " << _packetSize << std::endl;
	std::cout << std::endl;
}

void Sensor::StopAcquisition()
{
	if( _sensorState.SensorConnection != SensorConnectionState::SensorConnected )
	{
		return;
	}
	if( !m_isCapturing )
	{
		return;
	}
	//=======================================
	// stop the sensor
	//=======================================
	std::cout << "Stop acquisition" << std::endl;
	int ret = SR_API_StopAcquisition( _sensor.get() );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::WaitForAcquisitionCycle( int totalExpectedImages, int acquisitionTimeoutMs, int acquisitionTimeoutStepMs )
{
	std::cout << "Waiting for acquisition of " << totalExpectedImages << " image(s)" << std::endl;

	//=======================================
	// wait until requested number of images have been received
	//=======================================
	while( 0 < acquisitionTimeoutMs && !_sensorState.hasError )
	// skip loop on timeout
	// skip loop on sensor error
	{
		Sleep( acquisitionTimeoutStepMs );
		acquisitionTimeoutMs -= acquisitionTimeoutStepMs;

		ScopedLock guard( m_guard );

		if( static_cast<int>( _images.size() ) < totalExpectedImages )
		{  // continue loop on missing images
			continue;
		}

		if( 0 < totalExpectedImages )
		{
			if( !_images[totalExpectedImages - 1]->ContainsFullImages() )
			{  // wait until we got a full image
				continue;
			}
		}
		break;
	}
	std::cout << "Number of expected images: " << totalExpectedImages
	          << " Number of received images: " << _images.size() << std::endl;
	if( acquisitionTimeoutMs <= 0 )
	{
		SensorManager::HandleError( "FAIL: Acquisition timeout reached!" );
	}
	if( _sensorState.hasError )
	{
		SensorManager::HandleError( "FAIL: Sensor sent error message!" );
	}
}


void Sensor::WaitForAcquisitionCyclePointCloudMSR( int totalExpectedPointclouds,
                                                   int acquisitionTimeoutMs,
                                                   int acquisitionTimeoutStepMs )
{
	std::cout << "Waiting for acquisition of " << totalExpectedPointclouds << " point cloud(s)" << std::endl;

	//=======================================
	// wait until requested number of point clouds have been received
	//=======================================
	while( 0 < acquisitionTimeoutMs && !_sensorState.hasError )
	// skip loop on timeout
	// skip loop on sensor error
	{
		Sleep( acquisitionTimeoutStepMs );
		acquisitionTimeoutMs -= acquisitionTimeoutStepMs;

		ScopedLock guard( m_guard );

		if( _msrMode && totalExpectedPointclouds <= static_cast<int>( _pointCloudsMSR.size() ) )
		{  // break loop on received point clouds
			break;
		}
	}
	std::cout << "Number of expected point clouds: " << totalExpectedPointclouds
	          << " Number of received point clouds: " << _pointCloudsMSR.size() << std::endl;
	if( acquisitionTimeoutMs <= 0 )
	{
		SensorManager::HandleError( "FAIL: Acquisition timeout reached!" );
	}
	if( _sensorState.hasError )
	{
		SensorManager::HandleError( "FAIL: Sensor sent error message!" );
	}
}

void Sensor::PreparePointCloudsAcquisition( const std::string& filename,
                                            const float        transportResolution,
                                            const bool         saveAllPoints,
                                            const bool         saveAllAcquisitions,
                                            const uint32_t     acquisitionTimeoutMs,
                                            const uint32_t     acquisitionTimeoutStepMs )
{
	_pointCloudFilePrefix     = filename;
	_saveAllPoints            = saveAllPoints;
	_saveAllAcquisitions      = saveAllAcquisitions;
	_acquisitionTimeoutMs     = acquisitionTimeoutMs;
	_acquisitionTimeoutStepMs = acquisitionTimeoutStepMs;
	SetTransportResolution( transportResolution );

	// Get the width configured for the current acquisition:
	int32_t originX, width, originY, height;
	int     ret = SR_API_GetROI( _sensor.get(), &originX, &width, &originY, &height );
	SensorManager::HandleReturnCode( ret );
	_width = width;

	ret = SR_API_GetNumberOfProfilesToCapture( _sensor.get(), &_numberOfExpectedProfiles );
	SensorManager::HandleReturnCode( ret );

	std::cout << ">>> numberOfExpectedProfiles = " << _numberOfExpectedProfiles << std::endl;
	std::cout << ">>> width = " << _width << std::endl;
}

void Sensor::SaveLastPointCloudAcquisition()
{
	ProcessAcquiredPointClouds( true, _acquisitionCnt );
}

void Sensor::ProcessAcquiredPointClouds( const bool saveAcquiredData, const uint32_t count )
{
	ScopedLock guard( m_guard );

	if( saveAcquiredData )
	{  // Save the received Point Cloud data to file.
		SavePointCloudsToFile( count );
	}
}

void Sensor::RepeatedPointCloudAcquisition( const uint32_t repetitions )
{
	// 1.) Loop to repeat acquisition sequences:
	for( uint32_t m = 1; m < repetitions; m++ )
	{
		std::cout << std::endl << ">>> Acquisition sequence number = " << m << std::endl;
		AcquirePointClouds();
		ProcessAcquiredPointClouds( _saveAllAcquisitions, _acquisitionCnt );
	}
	// 2.) Last acquisition sequence:
	std::cout << std::endl << ">>> Acquisition sequence number = " << repetitions << std::endl;
	AcquirePointClouds();
}

void Sensor::PointCloudAcquisition()
{
	// Perform only one acquisition sequence:
	_acquisitionCnt = 1;
	AcquirePointClouds();
}


void Sensor::AcquirePointClouds()
{
	uint32_t timeout = _acquisitionTimeoutMs;
	while( 0 < timeout && !_sensorState.hasError && _numberOfCapturedProfiles < _numberOfExpectedProfiles )
	{
		std::cout << ".";
		Sleep( _acquisitionTimeoutStepMs );
		timeout -= _acquisitionTimeoutStepMs;
	}

	if( timeout <= 0 )
	{
		SensorManager::HandleError( "ERROR: Acquisition timeout occured!" );
	}
	if( _sensorState.hasError )
	{
		SensorManager::HandleError( "ERROR: Sensor sent error message!" );
	}
}

std::string Sensor::FactoryParameterSet( ParameterSet parameterSet )
{
	//=======================================
	// load and use default parameter set of the respective sensor series
	//=======================================
	char const* root             = std::getenv( "SmartRay" );
	std::string parameterSetPath = ( root ? root : "" );
	parameterSetPath += "\\SR_API\\sr_parameter_sets\\Pars_" + _sensorSeries + "\\" + _sensorSeries;
	// predefined live image parameter set for the ecco75 series as part of the installation
	_liveImageMode = false;
	switch( parameterSet )
	{
		case LiveImageParameterSet:
			parameterSetPath += "_Liveimage.par";
			_liveImageMode = true;
			break;
		case Snapshot3dParameterSet:
			parameterSetPath += "_3D_Snapshot.par";
			break;
		case Snapshot3dRepeatParameterSet:
			parameterSetPath += "_3D_Repeat_Snapshot.par";
			break;
		default:
			SensorManager::HandleError( "unsupported parameter set specified: " + parameterSet, true );
			break;
	}
	return parameterSetPath;
}

void Sensor::LoadParameterSet( ParameterSet parameterSet )
{
	LoadParameterSet( FactoryParameterSet( parameterSet ).c_str() );
}

void Sensor::LoadParameterSet( char const* parameterSet )
{
	// try to read the parameter set from the file
	std::cout << "try to read parameter set from file: " << parameterSet << std::endl;

	int ret = SR_API_LoadParameterSetFromFile( _sensor.get(), parameterSet );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::SaveParameterSet( ParameterSet parameterSet )
{
	SaveParameterSet( FactoryParameterSet( parameterSet ).c_str() );
}

void Sensor::SaveParameterSet( char const* parameterSet )
{
	// try to read the parameter set from the file
	std::cout << "try to write parameter set to file: " << parameterSet << std::endl;

	int ret = SR_API_SaveParameterSet( _sensor.get(), parameterSet );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::SetMetaDataExportEnable( bool enable )
{
	std::cout << "Meta-data export = " << enable << std::endl;
	int ret         = SR_API_SetMetaDataExportEnabled( _sensor.get(), enable );
	_metaDataExport = enable;

	// only ECCO 95
	SensorManager::HandleReturnCode( ret );
}

void Sensor::SendParameterSet()
{
	std::cout << "send the parameter set to the sensor." << std::endl;
	int ret = SR_API_SendParameterSetToSensor( _sensor.get() );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::GetAllParameters()
{
	GetRegionOfInterestParameters();

	int32_t numberofExposure = 0;
	int     ret              = SR_API_GetNumberOfExposureTimes( _sensor.get(), &numberofExposure );
	for( int i = 0; i < numberofExposure; i++ )
	{
		int exposureTime = 0;
		ret              = SR_API_GetExposureTime( _sensor.get(), i, &exposureTime );
		std::cout << "Sensor Index : " << _sensor->cam_index << " ::Exposure_Index : " << i
		          << " ::Exposure_Value : " << exposureTime << std::endl;
	}

	bool8_t gainEnable = false;
	int     gainValue  = 0;
	ret                = SR_API_GetGain( _sensor.get(), &gainEnable, &gainValue );
	std::cout << "Sensor Index : " << _sensor->cam_index << " Gain Enable :  " << gainEnable
	          << " Gain Value : " << gainValue << std::endl;

	AcquisitionMode imageAcqMode = AcquisitionMode_Snapshot;
	ret                          = SR_API_GetAcquisitionMode( _sensor.get(), &imageAcqMode );
	std::cout << "Sensor Index : " << _sensor->cam_index << " Image Acquistion mode " << imageAcqMode << std::endl;

	ImageAquisitionType imageType = ImageAquisitionType_ProfileIntensityLaserLineThickness;
	ret                           = SR_API_GetImageAcquisitionType( _sensor.get(), &imageType );
	std::cout << "Sensor Index : " << _sensor->cam_index << " Image Acquistion type " << imageType << std::endl;

	bool8_t smartXc = false;
	ret             = SR_API_GetSmartXccelerate( _sensor.get(), &smartXc );
	std::cout << "Sensor Index : " << _sensor->cam_index << " SmartXccelerate " << smartXc << std::endl;

	bool8_t readyForAcq = false;
	ret                 = SR_API_GetReadyForAcquisitionStatus( _sensor.get(), DigitalOutput_Channel2, &readyForAcq );
	std::cout << "Sensor Index : " << _sensor->cam_index << " Ready for Acquisition " << readyForAcq << std::endl;
}
void Sensor::SetMultiExposureMode( MultipleExposureMergeModeType merge_mode )
{
	std::cout << "set multi exposure merge mode." << std::endl;
	SR_API_SetMultiExposureMode( _sensor.get(), merge_mode );
}

void Sensor::LoadCalibrationDataFromSensor()
{
	static int err_cnt   = 0;
	static int err_cnt_t = 0;
	static int good_cnt  = 0;
	std::cout << "loading calibration from the sensor" << std::endl;
	try
	{
		int ret = SR_API_LoadCalibrationDataFromSensor( _sensor.get() );
		if( ret )
		{
			std::cout << " Error while load calibration (" << ret << "): " << ++err_cnt << std::endl;
		}
		else
		{
			std::cout << " Load calibration FILE OK: " << ++good_cnt << std::endl;
		}
		SensorManager::HandleReturnCode( ret );
	}
	catch( ... )
	{
		std::cout << " ERROR loading CALIB FILE: " << ++err_cnt_t << std::endl;
	}
}

void Sensor::LoadCalibrationDataFromFile( std::string fileName )
{
	char c_path[1024];
	strcpy( c_path, fileName.c_str() );
	std::cout << "loading calibration from the file : " << fileName << std::endl;
	int ret = SR_API_LoadCalibrationDataFromFile( _sensor.get(), c_path );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::ConfigureRegionOfInterestDivider( int divider )
{
	//=======================================
	// get sensor resolution
	//=======================================
	int sensorWidth  = 0;
	int sensorHeight = 0;
	std::cout << "requesting sensor resolution..." << std::endl;
	int ret = SR_API_GetSensorMaxDimensions( _sensor.get(), &sensorWidth, &sensorHeight );
	SensorManager::HandleReturnCode( ret );
	std::cout << "sensor resolution: " << sensorWidth << " x " << sensorHeight << std::endl;

	//=======================================
	// setup region of interest
	//=======================================
	int regionOfInterestX      = 0;
	int regionOfInterestY      = 0;
	int regionOfInterestWidth  = sensorWidth / divider;
	int regionOfInterestHeight = sensorHeight / divider;
	std::cout << "setting region of interest to " << regionOfInterestWidth << " x " << regionOfInterestHeight
	          << std::endl;
	ret = SR_API_SetROI(
	    _sensor.get(), regionOfInterestX, regionOfInterestWidth, regionOfInterestY, regionOfInterestHeight );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::ConfigureRegionOfInterest( int originX, int width, int originY, int height )
{
	//=======================================
	// setup region of interest
	//=======================================
	std::cout << "setting region of interest to OriginX:" << originX << "  width:" << width << "  OriginY:" << originY
	          << " height:" << height << std::endl;
	int ret = SR_API_SetROI( _sensor.get(), originX, width, originY, height );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::EnableSmartXpress()
{
	//=======================================
	// enable SmartXpress on the sensor
	//=======================================
	int ret = SR_API_SetSmartXpress( _sensor.get(), true );
	SensorManager::HandleReturnCode( ret );

	//=======================================
	// set a SmartXpress configuration file (*.sxp)
	//=======================================
	const char* configurationFilePath = ".\\SmartXpressSample.sxp";
	ret                               = SR_API_SetSmartXpressConfiguration( _sensor.get(), configurationFilePath );
	SensorManager::HandleReturnCode( ret );

	//=======================================
	// get the current SmartXpress configuration
	//=======================================
	char configuration[260];
	ret = SR_API_GetSmartXpressConfiguration( _sensor.get(), configuration, 260 );
	SensorManager::HandleReturnCode( ret );
	std::cout << "SmartXpress Configuration: " << configuration << std::endl;
}

void Sensor::DisableSmartXpress()
{
	//=======================================
	// disable SmartXpress on the sensor
	//=======================================
	int ret = SR_API_SetSmartXpress( _sensor.get(), false );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::EnableSmartXtract()
{
	//=======================================
	// set a SmartXtract configuration file (*.sxt)
	//=======================================
	std::string const reflection_filter = std::string( std::getenv( "SmartRay" ) )
	                                      + "\\SR_API\\smartxtract\\archive.sxt";
	int ret = SR_API_SetSmartXtractPreset( _sensor.get(), reflection_filter.c_str() );
	SensorManager::HandleReturnCode( ret );

	//=======================================
	// enable SmartXtract on the sensor
	//=======================================
	ret = SR_API_EnableSmartXtract( _sensor.get(), true );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::DisableSmartXtract()
{
	//=======================================
	// disable SmartXtract on the sensor
	//=======================================
	int ret = SR_API_EnableSmartXtract( _sensor.get(), false );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::EnableSmartXtractArchive()
{
	//=======================================
	// enable SmartXtract archiving
	//=======================================
	int ret = SR_API_ArchiveSmartXtractData( _sensor.get(), ".\\SmartXtractArchiveSample_archive.dat" );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::DisableSmartXtractArchive()
{
	//=======================================
	// disable SmartXtract archiving
	//=======================================
	int ret = SR_API_DisableArchiveSmartXtractData( _sensor.get() );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::SetSmartXtractAlgorithmType( SmartXtractAlgorithmType mode )
{
	SensorManager::HandleReturnCode( SR_API_SetSmartXtractAlgorithm( _sensor.get(), mode ) );
}

void Sensor::ShowsScanRate()
{
	//=======================================
	// shows the scan rate
	//=======================================
	int32_t scanRateMax = 0;
	int     ret         = SR_API_GetMaximumScanRate( _sensor.get(), &scanRateMax );
	std::cout << "Scan rate: " << scanRateMax << std::endl;
	SensorManager::HandleReturnCode( ret );
}

void Sensor::ConfigureExposureTimeMicroS( int exposureTimeMicroS )
{
	//=======================================
	// set the exposure time
	//=======================================
	std::cout << "set exposure time: " << exposureTimeMicroS << " Microseconds " << std::endl;

	int ret = SR_API_SetExposureTime( _sensor.get(), 0, exposureTimeMicroS );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::ConfigureDoubleExposureTimesMicroS( int exposureTime1MicroS, int exposureTime2MicroS )
{
	//=======================================
	// set the exposure time
	//=======================================
	bool enableDoubleExposure = true;
	std::cout << "enabling multiple exposure mode, exposure time 1: " << exposureTime1MicroS << " Microseconds "
	          << " time 2: " << exposureTime2MicroS << " Microseconds " << std::endl;

	// set number of exposure times
	int ret = SR_API_SetNumberOfExposureTimes( _sensor.get(), 2 );
	SensorManager::HandleReturnCode( ret );

	// set 1st exposure time
	ret = SR_API_SetExposureTime( _sensor.get(), 0, exposureTime1MicroS );
	SensorManager::HandleReturnCode( ret );

	// set 2nd exposure time (multiple exposure feature)
	ret = SR_API_SetExposureTime( _sensor.get(), 1, exposureTime2MicroS );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::ConfigureLaserMode( LaserMode lasermode )
{
	std::cout << "laser mode configured to " << lasermode << std::endl;
	int ret = SR_API_SetLaserMode( _sensor.get(), lasermode );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::ConfigureLaserBrightnessPercent( int laserPowerPercent )
{
	std::cout << "laser brightness configured to " << laserPowerPercent << " percent" << std::endl;
	int ret = SR_API_SetLaserBrightness( _sensor.get(), laserPowerPercent );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::ConfigureDataTriggerToFrequencyHz( int triggerFrequencyHz )
{
	//=======================================
	// configure data trigger (triggers that capture PIL')
	// Data Trigger Modes:
	//     1. Free Run
	//     2. Internal
	//     3. External
	//=======================================
	std::cout << "set data trigger mode to internal" << std::endl;
	int ret = SR_API_SetDataTriggerMode( _sensor.get(), DataTriggerMode_Internal );
	SensorManager::HandleReturnCode( ret );

	// configure internal data trigger frequency
	// Note: The internal data trigger frequency cannot be greater than the maximum scan rate achievable by the sensor
	//       in "Data Trigger Mode: Free Run", for the configured ROI and Exposure Time)
	std::cout << "set data trigger internal frequency: " << triggerFrequencyHz << " Hz" << std::endl;
	ret = SR_API_SetDataTriggerInternalFrequency( _sensor.get(), triggerFrequencyHz );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::ConfigureStartTriggerOnHardwareInput( StartTriggerSource input, bool enable )
{
	//=======================================
	// configure the start trigger
	// (triggers the start of a new acquisition cycle)
	//=======================================
	std::cout << "configure the acquisition start trigger" << std::endl;
	int ret = SR_API_SetStartTrigger( _sensor.get(), input, enable, TriggerEdgeMode_RisingEdge );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::Configure3DImageAquisition( ImageAquisitionType imageType, int numberOfProfiles )
{
	//=======================================
	// configure acquiring parameters manually
	//=======================================

	_liveImageMode = ( imageType == ImageAquisitionType_LiveImage );

	// set image type for provided sensor data
	std::cout << "configuring the image type to: " << imageType << std::endl;
	int ret = SR_API_SetImageAcquisitionType( _sensor.get(), imageType );
	SensorManager::HandleReturnCode( ret );

	std::cout << "configuring the number of profiles to be acquired: " << numberOfProfiles << std::endl;
	ret = SR_API_SetNumberOfProfilesToCapture( _sensor.get(), numberOfProfiles );
	SensorManager::HandleReturnCode( ret );
	ret = SR_API_SetPacketSize( _sensor.get(), 0 /* autopacketsize */ );
	SensorManager::HandleReturnCode( ret );
	// ECCO 95 only, 500ms default
	SR_API_SetPacketTimeOut( _sensor.get(), 500 );
}

void Sensor::SetZmapResolution( float lateralResolution, float verticalResolution )
{
	int ret = SR_API_SetZmapResolution( _sensor.get(), lateralResolution, verticalResolution );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::GetZmapResolution( float* lateralResolution, float* verticalResolution )
{
	int ret = SR_API_GetZmapResolution( _sensor.get(), lateralResolution, verticalResolution );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::ConfigureMSRMode( const std::string& registrationFilenameParameter )
{
	std::cout << "enable MSR mode" << std::endl;
	int ret = SR_API_MSR_EnableRegistration( true );
	SensorManager::HandleReturnCode( ret );

	std::string registrationFilename = registrationFilenameParameter;
	if( registrationFilename == "" )
	{
		registrationFilename = std::getenv( "SmartRay" );
		registrationFilename.append( "\\SR_Studio_4\\msr\\multi-sensor-registration\\transformation.xml" );
	}

	std::cout << "load registration file created during MSR registration mode:\n\t" << registrationFilename << "\n";
	/*Load the correct registration file path*/
	ret = SR_API_MSR_LoadRegistrationFile( registrationFilename.c_str() );
	SensorManager::HandleReturnCode( ret );

	ret = SR_API_MSR_CheckSettings();
	SensorManager::HandleReturnCode( ret );

	_msrMode = true;
}

std::vector<SR_3DPOINT> Sensor::CreatePointCloud( SensorImageData* imageData )
{
	std::cout << "creating a point cloud from the profile image..." << std::endl;

	//============================================================
	// create the point cloud from the profile image data
	//============================================================
	std::vector<SR_3DPOINT>          pointCloud( imageData->CurrentHeight * imageData->Width );
	PackagedImage<uint16_t>::Package image = imageData->ProfileImage.GetFullImage();
	int                              ret   = SR_API_CreatePointCloudMultipleProfile( _sensor.get(),
                                                      image.data(),
                                                      imageData->OriginX,
                                                      imageData->Width,
                                                      imageData->CurrentHeight,
                                                      pointCloud.data() );
	SensorManager::HandleReturnCode( ret );

	return pointCloud;
}

void Sensor::SavePointCloudToAscii( std::string filename,
                                    SR_3DPOINT* pointCloud,
                                    int         width,
                                    int         height,
                                    float       transportResolution,
                                    bool        removeNonWorld )
{
	std::cout << "saving point cloud to ASCII file: " << filename.c_str() << std::endl;
	std::ofstream output_file( filename + ".asc" );
	for( int y = 0; y < height; y++ )
		for( int x = 0; x < width; x++ )
		{
			if( !removeNonWorld || ( pointCloud[x + ( width * y )].x > NONWORLDMARK ) )
			{
				output_file << y * transportResolution << "\t" << pointCloud[x + ( width * y )].y << "\t"
				            << pointCloud[x + ( width * y )].z;
				output_file << std::endl;
			}
		}
	output_file.close();
}

void Sensor::GetSensorResolution()
{
	int sensorWidth  = 0;
	int sensorHeight = 0;
	std::cout << "requesting sensor resolution..." << std::endl;
	int ret = SR_API_GetSensorMaxDimensions( _sensor.get(), &sensorWidth, &sensorHeight );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Sensor resolution: " << sensorWidth << " x " << sensorHeight << std::endl;
}

void Sensor::GetSensorResolution( int& sensorWidth, int& sensorHeight )
{
	int ret = SR_API_GetSensorMaxDimensions( _sensor.get(), &sensorWidth, &sensorHeight );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::GetRegionOfInterestParameters()
{
	//=======================================
	// getter region of interest
	//=======================================
	int regionOfInterestX      = 0;
	int regionOfInterestY      = 0;
	int regionOfInterestWidth  = 0;
	int regionOfInterestHeight = 0;
	std::cout << "requesting region of interest " << std::endl;
	int ret = SR_API_GetROI(
	    _sensor.get(), &regionOfInterestX, &regionOfInterestWidth, &regionOfInterestY, &regionOfInterestHeight );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Region of interest: regionOfInterestX :" << regionOfInterestX
	          << " regionOfInterestWidth : " << regionOfInterestWidth << " regionOfInterestY : " << regionOfInterestY
	          << " regionOfInterestHeight : " << regionOfInterestHeight << std::endl;
}

void Sensor::SetRegionOfInterestParameters( int roiPosX, int roiWidth, int roiPosY, int roiHeight )
{
	int ret = SR_API_SetROI( _sensor.get(), roiPosX, roiWidth, roiPosY, roiHeight );
	SensorManager::HandleReturnCode( ret );
}

void Sensor::GetExposureParameters()
{
	//=======================================
	// getter gain
	//=======================================
	bool gainEnable = false;
	int  gainValue  = 0;
	std::cout << "requesting gain settings... " << std::endl;
	int ret = SR_API_GetGain( _sensor.get(), &gainEnable, &gainValue );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Gain settings : gainEnable :" << gainEnable << " gainValue : " << gainValue << std::endl;

	//=======================================
	// getter exposure
	//=======================================
	int exposureValue = 0;
	std::cout << "requesting exposure 1 value... " << std::endl;
	ret = SR_API_GetExposureTime( _sensor.get(), 0, &exposureValue );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Exposure 1 value : " << exposureValue << std::endl;
	std::cout << "requesting exposure 2 value... " << std::endl;
	ret = SR_API_GetExposureTime( _sensor.get(), 1, &exposureValue );
	if( ret != SUCCESS )
		std::cout << " Exposure 2 is not enabled " << std::endl;
	else
		std::cout << " Exposure 2 value : " << exposureValue << std::endl;
}

void Sensor::GetLaserParameters()
{
	//=======================================
	// getter laser parameters
	//=======================================
	LaserMode mode            = LaserMode_ContinousMode;
	bool      enable          = false;
	int       laserBrightness = 0;
	std::cout << "requesting laser power mode... " << std::endl;
	int ret = SR_API_GetLaserPower( _sensor.get(), &enable );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Laser Power  Enable : " << enable << std::endl;
	std::cout << "requesting laser mode... " << std::endl;
	ret = SR_API_GetLaserMode( _sensor.get(), &mode );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Laser Mode is: " << mode << std::endl;
	std::cout << "requesting laser brightness... " << std::endl;
	ret = SR_API_GetLaserBrightness( _sensor.get(), &laserBrightness );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Laser brightness : " << laserBrightness << std::endl;
}

void Sensor::GetStartTriggerParameters()
{
	//=======================================
	// getter Start Trigger parameters
	//=======================================

	StartTriggerSource source = StartTriggerSource_None;
	bool               enable = false;
	TriggerEdgeMode    edge   = TriggerEdgeMode_FallingEdge;
	std::cout << "requesting start trigger settings... " << std::endl;
	int ret = SR_API_GetStartTrigger( _sensor.get(), &source, &enable, &edge );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Start Trigger settings : ";
	std::cout << "Source : " << source << " Enable : " << enable << " Edge : " << edge << std::endl;
}

void Sensor::GetDataTriggerParameters()
{
	//=======================================
	// getter Data Trigger parameters
	//=======================================

	DataTriggerMode mode = DataTriggerMode_FreeRunning;
	std::cout << "requesting data trigger mode..." << std::endl;
	int ret = SR_API_GetDataTriggerMode( _sensor.get(), &mode );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Data Trigger mode : " << mode << std::endl;
	if( mode == DataTriggerMode_Internal )
	{
		int internalFrequency = 0;
		std::cout << " requesting data trigger internal frequency... " << std::endl;
		ret = SR_API_GetDataTriggerInternalFrequency( _sensor.get(), &internalFrequency );
		SensorManager::HandleReturnCode( ret );
		std::cout << " Internal frequency : " << internalFrequency << std::endl;
	}
	if( mode == DataTriggerMode_External )
	{
		DataTriggerSource datatriggersource = DataTriggerSource_Input1;
		std::cout << "requesting external data trigger source..." << std::endl;
		ret = SR_API_GetDataTriggerExternalTriggerSource( _sensor.get(), &datatriggersource );
		SensorManager::HandleReturnCode( ret );
		std::cout << " External data trigger source : " << datatriggersource << std::endl;
		if( datatriggersource == DataTriggerSource_Input1 || datatriggersource == DataTriggerSource_Input2
		    || datatriggersource == DataTriggerSource_Combined || datatriggersource == DataTriggerSource_QuadEncoder )
		{
			int             triggerdelay     = 0;
			int             triggerdivider   = 0;
			TriggerEdgeMode triggercondition = TriggerEdgeMode_Both;
			std::cout << "requesting external data trigger parameters..." << std::endl;
			ret = SR_API_GetDataTriggerExternalTriggerParameters( _sensor.get(),
			                                                      &triggerdivider,
			                                                      &triggerdelay,
			                                                      &triggercondition );
			SensorManager::HandleReturnCode( ret );
			std::cout << " External trigger parameters : ";
			std::cout << "Trigger divider : " << triggerdivider << "Trigger delay : " << triggerdelay
			          << "Trigger direction " << triggercondition;
		}
	}
}

void Sensor::GetReflectionFilterParameters()
{
	//=======================================
	// getter Reflection filter parameters
	//=======================================

	bool enableReflectionFilter = false;
	int  algorithm              = 0;
	int  presets                = 0;
	std::cout << " requesting reflection filter parameters... " << std::endl;
	int ret = SR_API_GetReflectionFilter( _sensor.get(), &enableReflectionFilter, &algorithm, &presets );
	std::cout << " Reflection filter parameters : ";
	std::cout << "Enable : " << enableReflectionFilter << " Algorithm : " << algorithm << "Preset : " << presets
	          << std::endl;
}

void Sensor::GetAcquisitionParameters()
{
	//=======================================
	// getter Acquisition parameters
	//=======================================
	ImageAquisitionType acquisitionType = ImageAquisitionType_Profile;
	std::cout << " requesting image acquisition type... " << std::endl;
	int ret = SR_API_GetImageAcquisitionType( _sensor.get(), &acquisitionType );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Image acquisition type : " << acquisitionType << std::endl;
	AcquisitionMode acquisitionMode = AcquisitionMode_Snapshot;
	std::cout << " requesting image acquisition mode... " << std::endl;
	ret = SR_API_GetAcquisitionMode( _sensor.get(), &acquisitionMode );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Image acquisition mode : " << acquisitionMode << std::endl;
	uint32_t numberofProfile = 0;
	uint32_t packetSize      = 0;
	uint32_t packetTimeout   = 0;
	std::cout << " requesting number of profiles to capture and packet size... " << std::endl;
	ret = SR_API_GetNumberOfProfilesToCapture( _sensor.get(), &numberofProfile );
	SensorManager::HandleReturnCode( ret );
	ret = SR_API_GetPacketSize( _sensor.get(), &packetSize );
	SensorManager::HandleReturnCode( ret );
	// ECCO 95 only, 500ms default timeout
	ret = SR_API_GetPacketTimeOut( _sensor.get(), &packetTimeout );
	std::cout << " Number of profiles to capture : " << numberofProfile << " Packet size : " << packetSize
	          << " Packet timeout : " << packetTimeout << std::endl;
	int laserlineThreshold = 0;
	std::cout << " requesting laser line threshold... " << std::endl;
	ret = SR_API_Get3DLaserLineBrightnessThreshold( _sensor.get(), 0, &laserlineThreshold );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Laser line threshold for exposure index 0 : " << laserlineThreshold << std::endl;
	ret = SR_API_Get3DLaserLineBrightnessThreshold( _sensor.get(), 1, &laserlineThreshold );
	std::cout << " Laser line threshold for exposure index 1 : " << laserlineThreshold << std::endl;
}

void Sensor::GetSensorInformation()
{
	//=======================================
	// getter sensor information
	//=======================================

	char* apiVersion = NULL;
	std::cout << "requesting API version...  " << std::endl;
	int ret = SR_API_GetAPIVersion( &apiVersion );
	SensorManager::HandleReturnCode( ret );
	std::cout << " API version : " << apiVersion << std::endl;

	char macAdd[MACSIZE];
	std::cout << "requesting sensor mac address...  " << std::endl;
	ret = SR_API_GetSensorMacAddress( _sensor.get(), macAdd );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Sensor mac address : " << macAdd << std::endl;

	char partNumber[256];
	char modelName[256];
	std::cout << "requesting sensor model name and part number...  " << std::endl;
	ret = SR_API_GetSensorModelName( _sensor.get(), modelName, partNumber );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Sensor model name : " << modelName << " Part number : " << partNumber << std::endl;

	char firmwareVersion[CAMVERSIONSIZE];
	std::cout << "requesting sensor firmware version...  " << std::endl;
	ret = SR_API_GetSensorFirmwareVersion( _sensor.get(), firmwareVersion );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Sensor firmware version : " << firmwareVersion << std::endl;

	char serialNumber[SERIALSIZE];
	std::cout << "requesting sensor serial number...  " << std::endl;
	ret = SR_API_GetSensorSerialNumber( _sensor.get(), serialNumber );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Sensor serial number : " << serialNumber << std::endl;

	int originX = 0;
	int originY = 0;
	std::cout << "requesting sensor origin...  " << std::endl;
	ret = SR_API_GetSensorOrigin( _sensor.get(), &originX, &originY );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Sensor origin ";
	std::cout << "OriginX : " << originX << " OriginY : " << originY << std::endl;

	int granualarityX = 0;
	int granualarityY = 0;
	std::cout << "requesting sensor granularity...  " << std::endl;
	ret = SR_API_GetSensorGranularity( _sensor.get(), &granualarityX, &granualarityY );
	SensorManager::HandleReturnCode( ret );
	std::cout << " Sensor granularity ";
	std::cout << "GranualarityX : " << granualarityX << " GranualarityY : " << granualarityY << std::endl;

	int minMeasurementRange = 0;
	int maxMeasurementRange = 0;
	std::cout << "requesting sensor measurement range...  " << std::endl;
	ret = SR_API_GetMeasurementRange( _sensor.get(), &minMeasurementRange, &maxMeasurementRange );
	std::cout << " Sensor measurement range ";
	std::cout << "Minimum Measurement Range : " << minMeasurementRange
	          << " Maximum Measurement Range : " << maxMeasurementRange << std::endl;

	float lateralResolution  = 0;
	float verticalResolution = 0;
	std::cout << "requesting ZIL resolution...  " << std::endl;
	ret = SR_API_GetZmapResolution( _sensor.get(), &lateralResolution, &verticalResolution );
	std::cout << " ZIL resolution ";
	std::cout << "Lateral Resolution : " << lateralResolution << " Vertical Resolution : " << verticalResolution
	          << std::endl;
}

// Set pitch angle of sensor.
void Sensor::SetTiltCorrectionPitch( float degree )
{
	int ret = SR_API_SetTiltCorrectionPitch( _sensor.get(), degree );
	SensorManager::HandleReturnCode( ret );
}

// Set transport resolution of sensor.
void Sensor::SetTransportResolution( float transportResolution )
{
	int ret = SR_API_SetTransportResolution( _sensor.get(), transportResolution );
	SensorManager::HandleReturnCode( ret );
}

float Sensor::GetTransportResolution()
{
	float transportResolution = 0.0f;
	SR_API_GetTransportResolution( _sensor.get(), &transportResolution );
	return transportResolution;
}

void Sensor::SetSmartXactMode( int mode )
{
	int ret = SR_API_SetSmartXactMode( _sensor.get(), mode );
	SensorManager::HandleReturnCode( ret );
}

int Sensor::GetSmartXactMode()
{
	int32_t mode = 0;
	SR_API_GetSmartXactMode( _sensor.get(), &mode );
	return mode;
}
