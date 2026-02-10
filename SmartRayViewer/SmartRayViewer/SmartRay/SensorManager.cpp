
#include "SensorManager.h"
#include "SampleRunner.h"
#include "SensorImageData.h"

#include <iostream>
#include <sstream>
#include <string>

std::vector<SharedPtr<Sensor>> SensorManager::_sensors;

int SensorManager::ApiStatusCallback( SRSensor*      sensorObject,
                                      MessageType    msgType,
                                      SubMessageType subMsgType,
                                      int            msgData,
                                      char*          msg )
{
	// check for message data available
	if( !msg )
	{
		return -1;
	}

	// try to match the sensor object
	SharedPtr<Sensor> sensor = TryGetSensor( sensorObject );
	if( !sensor )
	{
		return -1;
	}

	// handle Ethernet connection messages
	switch( msgType )
	{
		case MessageType_Connection:
			sensor->_sensorState.LastConnectionMessage = msg;
			// TCP/IP connection established
			switch( subMsgType )
			{
				case SubMessageType_Connection_SensorConnected:
					sensor->_sensorState.SensorConnection = SensorConnected;
					break;
				default:
					sensor->_sensorState.SensorConnection = SensorDisconnected;
					break;
			}
			break;
		case MessageType_Info:
			sensor->_sensorState.LastInfoMessage = msg;
			break;
		case MessageType_Error:
			sensor->_sensorState.hasError         = true;
			sensor->_sensorState.LastErrorMessage = msg;
			std::cout << "Received error message from API with subMsgType=" << subMsgType << " and msgData=" << msgData
			          << ":" << std::endl;
			std::cout << "  '" << msg << "'" << std::endl;
			break;
		case MessageType_Data:
			switch( subMsgType )
			{
				case SubMessageType_Data_Io:
					// can be used to track the alive status of the sensor(Not available for ECCO 75)
					// std::cout << "IO Status Changed.";
					break;
				default:
					std::cout << __FUNCTION__ << ": Unknown subMsgType\n";
					break;
			}
			break;
		default:
			std::cout << __FUNCTION__ << ": Unknown msgType\n";
			break;
	}
	return 0;
}


int SensorManager::LiveImageCallback( SRSensor*     sensorObject,
                                      ImageDataType imageType,
                                      int           originX,
                                      int           height,
                                      int           width,
                                      uint8_t*      liveImage )
{
	// try to match the sensor object
	SharedPtr<Sensor> sensor = TryGetSensor( sensorObject );
	if( !sensor )
	{
		return -1;
	}

	std::cout << "*** Live image package " << ++( sensor->_sensorState.ImagePackageCounter )
	          << " received for sensor: " << sensorObject->name << " height: " << height << " width: " << width
	          << std::endl;

	// collect image data by adding the provided data package
	SharedPtr<SensorImageData> imageData = sensor->AddImageData( width, height, imageType, originX );
	imageData->LiveImage.AddPacket( liveImage, width * height );

	std::cout << "\n\n";
	return 0;
}

void SensorManager::AddMetaData( Sensor& sensor, uint32_t numberElements, size_t sizeExtDataEntry, void* extData )
{
	if( sensor.GetMetaDataExportEnable() && sizeExtDataEntry == sizeof( SR_MetaData ) )
	{
		sensor.AddMetaData( numberElements, (SR_MetaData*)extData );
	}
}

int SensorManager::PilImageCallback( SRSensor*     sensorObject,
                                     ImageDataType imageType,
                                     int           originX,
                                     int           height,
                                     int           width,
                                     uint16_t*     profileImage,
                                     uint16_t*     intensityImage,
                                     uint16_t*     lltImage,
                                     int           numExtData,
                                     void*         extData )
{
	// Check if Profile Value of Line 500 > 3800...
	static int save_img_cnt[2] = { 0, 0 };
	static int paket_cnt[2]    = { 0, 0 };

	SharedPtr<Sensor> sensor = TryGetSensor( sensorObject );
	if( !sensor )
	{
		return -1;
	}

	std::cout << "*** Profile image package " << ++( sensor->_sensorState.ImagePackageCounter )
	          << " received for sensor: " << sensorObject->name << " width: " << width << " height: " << height
	          << std::endl;

	// Apply post processing to profile image
	// Median Filter with kernel size 5x5
	int kernelSizeX  = 5;
	int kernelSizeY  = 5;
	int excludeZeros = 1;
	int ret = SR_API_MedianFilter( kernelSizeX, kernelSizeY, excludeZeros, width, height, profileImage, profileImage );

	// Image smoothing filter
	ret = SR_API_SmoothImage( profileImage, width, height, SmoothingPresets::SMOOTH_STRONG );

	// collect image data by adding the provided data package
	int size = width * height;

	SharedPtr<SensorImageData> imageData = sensor->AddImageData( width, height, imageType, originX );
	switch( imageType )
	{
		case ImageDataType_Profile:
			std::cout << "(profile image only)";
			imageData->ProfileImage.AddPacket( profileImage, size );
			break;

		case ImageDataType_Intensity:
			std::cout << "(intensity image only)";
			imageData->IntensityImage.AddPacket( intensityImage, size );
			break;

		case ImageDataType_ProfileIntensity:
			std::cout << "(profile & intensity image)";
			imageData->ProfileImage.AddPacket( profileImage, size );
			imageData->IntensityImage.AddPacket( intensityImage, size );
			break;

		case ImageDataType_ProfileIntensityLaserLineThickness:
			std::cout << "(profile, intensity and laser line thickness image)";
			imageData->ProfileImage.AddPacket( profileImage, size );
			imageData->IntensityImage.AddPacket( intensityImage, size );
			imageData->LaserLineThicknessImage.AddPacket( lltImage, size );
			break;

		default:
			break;
	}
	std::cout << std::endl << std::endl;

	AddMetaData( *sensor, height, numExtData, extData );

	return 0;
}

int SensorManager::ZilImageCallback( SRSensor*     sensorObject,
                                     ImageDataType imageType,
                                     int           height,
                                     int           width,
                                     float         verticalRes,
                                     float         horizontalRes,
                                     uint16_t*     zMap,
                                     uint16_t*     intensityZmap,
                                     uint16_t*     lltZmap,
                                     float         originYMillimeters,
                                     int           numExtData,
                                     void*         extData )
{
	// try to match the sensor object
	SharedPtr<Sensor> sensor = TryGetSensor( sensorObject );
	if( sensor == NULL )
		return -1;

	std::cout << "*** ZMap image packet " << ++( sensor->_sensorState.ImagePackageCounter )
	          << " received for sensor: " << sensorObject->name << " width: " << width << " height: " << height
	          << std::endl;
	std::cout << "vertical Resolution [mm]: " << verticalRes << " horizontal Resolution [mm]: " << horizontalRes
	          << std::endl;

	// Apply post processing to ZMap image
	// Median Filter with kernel size 5x5
	int kernelSizeX  = 5;
	int kernelSizeY  = 5;
	int excludeZeros = 1;
	int ret          = SR_API_MedianFilter( kernelSizeX, kernelSizeY, excludeZeros, width, height, zMap, zMap );

	// Image smoothing filter
	ret = SR_API_SmoothImage( zMap, width, height, SmoothingPresets::SMOOTH_STRONG );


	// collect image data by adding the provided data package
	int                        size      = width * height;
	SharedPtr<SensorImageData> imageData = sensor->AddImageData( width, height, imageType, 0, originYMillimeters );
	switch( imageType )
	{
		case ImageDataType_ZMap:
			std::cout << "(zmap image only)";
			imageData->ZMapImage.AddPacket( zMap, size );
			break;

		case ImageDataType_ZMapIntensity:
			std::cout << "(zmap & intensity zmap image)";
			imageData->ZMapImage.AddPacket( zMap, size );
			imageData->ZMapIntensityImage.AddPacket( intensityZmap, size );
			break;

		case ImageDataType_ZMapIntensityLaserLineThickness:
			std::cout << "(zmap, intensity zmap and laser line thickness zmap image)";
			imageData->ZMapImage.AddPacket( zMap, size );
			imageData->ZMapIntensityImage.AddPacket( intensityZmap, size );
			imageData->ZMapLaserLineThicknessImage.AddPacket( lltZmap, size );
			break;

		default:
			break;
	}
	std::cout << std::endl << std::endl;

	AddMetaData( *sensor, height, numExtData, extData );

	return 0;
}

int SensorManager::PointCloudCallback( SRSensor*     sensorObject,
                                       ImageDataType dattyp,
                                       uint32_t      numPoints,
                                       uint32_t      numProfile,
                                       SR_3DPOINT*   point_cloud,
                                       uint16_t*     intensity,
                                       uint16_t*     laserlinethickness,
                                       uint32_t*     profileIdx,
                                       uint32_t*     columnIdx,
                                       uint32_t      numExtData,
                                       void*         extData )
{
	// try to match the sensor object
	SharedPtr<Sensor> sensor = TryGetSensor( sensorObject );
	if( !sensor )
	{
		std::cout << __FUNCTION__ " Error: No sensor found for: " << sensorObject;
		return -1;
	}

	sensor->AddPointCloudData(
	    numPoints, numProfile, point_cloud, intensity, laserlinethickness, profileIdx, columnIdx );

	AddMetaData( *sensor, numProfile, numExtData, extData );

	return 0;
}

int SensorManager::MSRPointCloudCallback( SRSensor*   sensorObject,
                                          uint32_t    dattyp,
                                          uint32_t    numPoints,
                                          SR_3DPOINT* point_cloud,
                                          uint16_t*   intensity,
                                          uint16_t*   laserlinethickness,
                                          uint32_t*   sensorIdx,
                                          uint32_t*   profileIdx,
                                          uint32_t*   pointIdx,
                                          uint32_t    numMSRExtData,
                                          void*       extMSRData )
{
	// try to match the sensor object
	SharedPtr<Sensor> sensor = TryGetSensor( sensorObject );
	if( !sensor )
	{
		return -1;
	}

	std::cout << "*** MSR point cloud"
	          << " received for sensor: " << sensorObject->name << " numPoints: " << numPoints
	          << " numMSRExtData: " << numMSRExtData << std::endl;

	sensor->AddMSRPointCloudData(
	    numPoints, point_cloud, intensity, laserlinethickness, sensorIdx, profileIdx, pointIdx );

	return 0;
}

SharedPtr<Sensor> SensorManager::TryGetSensor( SRSensor* sensorObject )
{
	for( size_t i = 0; i < _sensors.size(); ++i )
	{
		if( _sensors[i]->_sensor.get() == sensorObject )
		{
			return _sensors[i];
		}
	}
	return nullptr;
}

bool SensorManager::HandleReturnCode( int apiReturnCode, bool fatal )
{
	if( SUCCESS == apiReturnCode )
	{
		return true;
	}

	// try to get error string from api
	char* apiErrorString = nullptr;
	SR_API_GetErrorMsg( apiReturnCode, &apiErrorString );

	std::stringstream errorText;
	if( apiErrorString )
	{
		errorText << apiErrorString << " Error code:" << apiReturnCode;
	}
	else
	{
		errorText << " Smartray API call failed with error code: " << apiReturnCode;
	}

	HandleError( errorText.str(), fatal );

	return false;
}

bool SensorManager::HandleReturnCode( int apiReturnCode )
{
	return HandleReturnCode( apiReturnCode, true );
}

void SensorManager::HandleError( std::string text, bool fatal )
{
	std::string const prefix    = fatal ? "FATAL: " : "ERROR: ";
	std::string const error_msg = prefix + text;
	std::cout << error_msg << std::endl;
	if( !fatal )
	{
		return;
	}
	for( size_t i = 0; i < _sensors.size(); ++i )
	{
		_sensors[i]->StopAcquisition();
		_sensors[i]->Disconnect();
	}
	_sensors.clear();
	SR_API_Exit();
	throw FatalErrorException( error_msg );
}

void SensorManager::InitApi( bool useMSRPointCloudCB )
{
	int ret;

	//=======================================
	// API Init
	//=======================================
	ret = SR_API_Initalize( SensorManager::ApiStatusCallback );
	HandleReturnCode( ret );

	//=======================================
	// get API version
	//=======================================
	char* apiVersion = NULL;
	ret              = SR_API_GetAPIVersion( &apiVersion );
	HandleReturnCode( ret );
	std::cout << "using Smartray API " << apiVersion << std::endl;

	//=======================================
	// register image callbacks
	//=======================================
	// register live image callback
	SR_API_RegisterLiveImageCB( &SensorManager::LiveImageCallback );
	// register PIL image callback
	SR_API_RegisterPilImageCB( &SensorManager::PilImageCallback );
	// register Point Cloud callback
	SR_API_RegisterPointCloudCB( &SensorManager::PointCloudCallback );

	if( !useMSRPointCloudCB )
	{
		// default: register ZIL callback
		SR_API_RegisterZilImageCB( &SensorManager::ZilImageCallback );
	}
	else
	{
		// register point cloud callback for MSR
		SR_API_RegisterMSRPointCloudCB( &SensorManager::MSRPointCloudCallback );
	}
}

void SensorManager::DeinitApi()
{
	//=======================================
	// teardown API
	//=======================================
	std::cout << "teardown API" << std::endl;
	int ret = SR_API_Exit();
	HandleReturnCode( ret );
}

SharedPtr<Sensor> SensorManager::CreateSensor( std::string name, const char* ipAddress, unsigned short port )
{
	_sensors.push_back( new Sensor( name, static_cast<int>( _sensors.size() ), ipAddress, port ) );
	return _sensors.back();
}

SensorManager::SensorManager( bool useMSRPointCloudCB )
{
	InitApi( useMSRPointCloudCB );
}

//
// Dtor
//
SensorManager::~SensorManager()
{
	DeinitApi();

	_sensors.clear();
}
