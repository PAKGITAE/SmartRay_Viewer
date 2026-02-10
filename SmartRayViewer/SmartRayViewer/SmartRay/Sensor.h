#pragma once
#include "SR_API_public.h"

#include "SensorImageData.h"
#include "SensorPointCloud.h"
#include "SensorPointcloudMSR.h"
#include "SensorState.h"
#include "locking.h"

#include <string.h>
#include <ctime>
#include <iostream>

#include <Windows.h>

static const float DEFAULT_RESOLUTION =
    (float)0.100000;  // Default x-axis resolution (transport resolution) for point cloud data.
static const float DEFAULT_PITCHANGLE = (float)22.5;  // Default pitch angle for tilt correction for point cloud data.
static const bool  SAVE_ALL_POINTS    = true;
static const bool  SAVE_ONLY_VALID_POINTS     = false;
static const bool  SAVE_ALL_ACQUISITIONS      = true;
static const bool  SAVE_ONLY_LAST_ACQUISITION = false;

//
// sample class for Smartray sensor control
//
class Sensor
{
	friend class SensorManager;

public:
	//
	// available parameter sets to configure the sensor
	//
	enum ParameterSet
	{
		UndefinedParameterSet        = 0,
		LiveImageParameterSet        = 1,
		Snapshot3dParameterSet       = 2,
		Snapshot3dRepeatParameterSet = 3,
	};

	typedef std::vector<SR_MetaData> MetaData;

protected:
	// sensor description object
	SharedPtr<SRSensor> _sensor;

	// provides the state of the sensor (updated by API callback)
	SensorState _sensorState;

	// the series of the sensor, ECCO 55, ECCO 75...
	std::string _sensorSeries;

	// number of profiles (lines) to be captured with the currect aquisition.
	uint32_t _numberOfExpectedProfiles;
	// Total number of profiles captured in the currect aquisition.
	uint32_t _numberOfCapturedProfiles;
	// Width of the configured ROI for the currect aquisition.
	int32_t _width;

	// number of profiles (lines) which are contained in one sensor data package
	uint32_t _packetSize;

	// packet timeout in ms
	uint32_t _packetTimeout;

	// true when live images shall be acquired
	bool _liveImageMode;

	// received sensor image data
	std::vector<SharedPtr<SensorImageData>> _images;
	// Received MSR point clouds.
	std::vector<SharedPtr<SensorPointcloudMSR>> _pointCloudsMSR;
	CriticalSectionEx                           m_guard;

	// received sensor meta data
	std::vector<SharedPtr<MetaData>> _meta_data;

	// true when MSR mode enabled
	bool _msrMode;
	bool _metaDataExport;

	// Point Clouds received from sensor via the callback function.
	// Access to this vector shall be done using the _sensorDataLock (due to the
	// concurrent access from SR_API.dll through the callback).
	std::vector<PointCloud> _cbPointClouds;

	// Filename-prefix, to store Point CLoud data.
	std::string _pointCloudFilePrefix;

	// true: in case of repeated acquisitions, all acquired data are stored.
	// false: only the data of the last acquisition is stored.
	bool _saveAllAcquisitions;

	// true: all points of the Point Cloud are stored in the file
	// false: only the valid points of the Point Cloud are stored in the file.
	bool _saveAllPoints;

	// Acquisition guard timer.
	uint32_t _acquisitionTimeoutMs;

	// Step to check and decrementing the guard timer.
	uint32_t _acquisitionTimeoutStepMs;

	// Acquisition counter.
	uint32_t _acquisitionCnt;

private:
	bool m_isCapturing;

	//
	// get image data object, so image packets can be added
	//
	SharedPtr<SensorImageData> AddImageData( int           width,
	                                         int           height,
	                                         ImageDataType imageType,
	                                         int           originX            = 0,
	                                         float         originYMillimeters = 0 );

	//
	// add meta data
	//
	void AddMetaData( int height, SR_MetaData* meta_data );

	//
	// Add a Point Cloud object.
	//
	void AddPointCloud( uint32_t    numPoints,
	                    uint32_t    numProfile,
	                    SR_3DPOINT* point_cloud,
	                    uint16_t*   intensity,
	                    uint16_t*   laserlinethickness,
	                    uint32_t*   profileIdx,
	                    uint32_t*   columnIdx );

	//
	// add MSR point cloud data
	//
	void AddMSRPointCloudData( uint32_t    numPoints,
	                           SR_3DPOINT* point_cloud,
	                           uint16_t*   intensity,
	                           uint16_t*   laserlinethickness,
	                           uint32_t*   sensorIdx,
	                           uint32_t*   profileIdx,
	                           uint32_t*   pointIdx );

	//
	// Acquire Point Cloud.
	//
	void AcquirePointClouds();

	//
	// Process acquired Point Cloud data.
	//
	void ProcessAcquiredPointClouds( const bool saveAcquiredData, const uint32_t count );

	void clear();

public:
	//
	// Ctor
	//
	Sensor( std::string    name,
	        int            multiSensorIndex = 0,
	        const char*    ipAddress        = DEFAULT_IP_ADR,
	        unsigned short port             = DEFAULT_PORT_NUM );

	//
	// Dtor
	//
	~Sensor();

	//
	// connect to the sensor
	//
	void Connect();

	//
	// disconnect from the sensor
	//
	void Disconnect();

	void clearBuffer();
	void clearReceivedImages( size_t count );

	//
	// extract sensor series from the sensor model name which will be used to set parameter set path
	//
	void GetSensorSeries();

	//
	// start data acquisition
	//
	void StartAcquisition();

	//
	// stop data acquisition
	//
	void StopAcquisition();

	bool isCapturing() const
	{
		return m_isCapturing;
	}

	//
	// Add Point Cloud data.
	//
	void AddPointCloudData( const uint32_t numPoints,
	                        const uint32_t numProfile,
	                        SR_3DPOINT*    point_cloud,
	                        uint16_t*      intensity,
	                        uint16_t*      laserlinethickness,
	                        uint32_t*      profileIdx,
	                        uint32_t*      columnIdx );

	//
	// returns a copy of the current sensor state
	//
	const SensorState GetSensorState();

	//
	// returns the most recent image data if available or NULL
	//
	SharedPtr<SensorImageData> GetLastImageData();

	//
	// Saves the most recent Point Cloud in a file.
	//
	void SavePointCloudsToFile( const uint32_t suffix );

	//
	// returns the most recent MSR point cloud if available or NULL
	//
	SharedPtr<SensorPointcloudMSR> GetLastPointcloudMSR();

	//
	// wait until the number of expected images has been received
	//
	void WaitForAcquisitionCycle( int totalExpectedImages,
	                              int acquisitionTimeoutMs     = 15000,
	                              int acquisitionTimeoutStepMs = 100 );

	//
	// Acquisition sequence of the expected Point Cloud profiles.
	//
	void PointCloudAcquisition();

	//
	// Repeat acquisitions of Point Clouds.
	//
	void RepeatedPointCloudAcquisition( const uint32_t repetitions );

	//
	// Prepare acquision of Point Clouds.
	//
	// filename                     : the filename-prefix to save the Point Cloud data will be saved.
	// transportResolution          : x-axis resolution for point cloud data.
	// saveAllAcquisitions          : Indicates if the data of all repeated acquisitions shall be stored in files.
	// saveAllPoints                : Indicates if all points shall be stored in output file (true) or only the valid
	// ones (false). acquisitionTimeoutMs         : Acquisition guard timer. acquisitionTimeoutStepMs     : Step to
	// check and decrementing the guard timer.
	//
	void PreparePointCloudsAcquisition( const std::string& filename,
	                                    const float        transportResolution,
	                                    const bool         saveAllPoints            = SAVE_ONLY_VALID_POINTS,
	                                    const bool         saveAllAcquisitions      = SAVE_ONLY_LAST_ACQUISITION,
	                                    const uint32_t     acquisitionTimeoutMs     = 15000,
	                                    const uint32_t     acquisitionTimeoutStepMs = 50 );

	//
	// wait until the number of expected point clouds have been received
	//
	void WaitForAcquisitionCyclePointCloudMSR( int totalExpectedPointclouds,
	                                           int acquisitionTimeoutMs     = 15000,
	                                           int acquisitionTimeoutStepMs = 100 );

	//
	// load a certain parameter set to reconfigure the sensor
	//
	std::string Sensor::FactoryParameterSet( ParameterSet parameterSet );
	void        LoadParameterSet( ParameterSet parameterSet );
	void        LoadParameterSet( char const* parameterSet );
	void        SaveParameterSet( ParameterSet parameterSet );
	void        SaveParameterSet( char const* parameterSet );

	//
	// send a parameter set to the sensor
	//
	void SendParameterSet();

	//
	// set multi exposure merge mode
	//
	void SetMultiExposureMode( MultipleExposureMergeModeType merge_mode );

	//
	// enable meta data in callback
	//
	void SetMetaDataExportEnable( bool enable );

	//
	// loading a calibration file is necessary for creating point cloud and ZIL
	//
	void LoadCalibrationDataFromSensor();

	//
	// loading a calibration file is necessary for creating point cloud and ZIL from file
	//
	void LoadCalibrationDataFromFile( std::string fileName );

	//
	// configures the region of interest to a half of the full size
	//
	void ConfigureRegionOfInterestDivider( int divider = 2 );

	//
	// configures Region Of Interest
	//
	void ConfigureRegionOfInterest( int originX, int width, int originY, int height );

	//
	// enables SmartXpress
	//
	void EnableSmartXpress();

	//
	// disables SmartXpress
	//
	void DisableSmartXpress();

	//
	// enables SmartXtract
	//
	void EnableSmartXtract();

	//
	// disables SmartXtract
	//
	void DisableSmartXtract();

	//
	// enables SmartXtract archiving
	//
	void EnableSmartXtractArchive();

	//
	// disables SmartXtract archiving
	//
	void DisableSmartXtractArchive();

	// @brief: Set exposure mode for reflection filter
	// @pre:   Valid reflection has to be set before.
	void SetSmartXtractAlgorithmType( SmartXtractAlgorithmType mode );

	//
	// shows scan rate
	//
	void ShowsScanRate();

	//
	// configures the exposure time
	//
	void ConfigureExposureTimeMicroS( int exposureTimeMicroS = 1000 );

	//
	// configures the exposure times (activates double exposure)
	//
	void ConfigureDoubleExposureTimesMicroS( int exposureTime1MicroS = 1000, int exposureTime2MicroS = 2000 );

	//
	// configure the laser mode
	//
	void ConfigureLaserMode( LaserMode lasermode );

	//
	// configure the laser parameters
	//
	void ConfigureLaserBrightnessPercent( int laserPowerPercent = 100 );

	//
	// configure an trigger mode to internal data trigger
	//
	void ConfigureDataTriggerToFrequencyHz( int triggerFrequencyHz = 10 );

	//
	// configure the start trigger for acquisition cycle
	//
	void ConfigureStartTriggerOnHardwareInput( StartTriggerSource slot, bool enable = true );

	//
	// configure 3D image acquisition type and number of profiles to capture
	//
	void Configure3DImageAquisition( ImageAquisitionType imageType, int numberOfProfiles = 100 );

	//
	// create a point cloud from profile image data
	//
	std::vector<SR_3DPOINT> CreatePointCloud( SensorImageData* profileImageData );

	//
	// configure MSR mode
	//
	void ConfigureMSRMode( const std::string& registrationFilename = "" );

	//
	// saves the point cloud data to the an ascii file
	//
	static void SavePointCloudToAscii( std::string filename,
	                                   SR_3DPOINT* pointCloud,
	                                   int         width,
	                                   int         height,
	                                   float       transportResolution = 0.1,
	                                   bool        removeNonWorld      = true );

	//
	// call getter & display the sensor resolution
	//
	void GetSensorResolution();

	//
	// call getter & return the sensor resolution
	//
	void GetSensorResolution( int& sensorWidth, int& sensorHeight );

	//
	// call getter & display the region of interest parameters
	//
	void GetRegionOfInterestParameters();

	//
	// set the region of interest parameters
	//
	void SetRegionOfInterestParameters( int roiPosX, int roiWidth, int roiPosY, int roiHeight );

	//
	// call getter & display the exposure times parameters
	//
	void GetExposureParameters();

	//
	// call getter & display the laser parameters
	//
	void GetLaserParameters();

	//
	// call getter & display start trigger parameters
	//
	void GetStartTriggerParameters();

	//
	// call getter & display the data trigger parameters
	//
	void GetDataTriggerParameters();

	//
	// call getter & display the reflection filter parameters
	//
	void GetReflectionFilterParameters();

	//
	// call getter & display the sensor acquisition parameters
	//
	void GetAcquisitionParameters();

	//
	// call getter & display the sensor information
	//
	void GetSensorInformation();

	//
	// Save the data of the last Point Cloud acquisition.
	//
	void SaveLastPointCloudAcquisition();

	//
	// get meta data export enable flag
	//
	inline bool GetMetaDataExportEnable()
	{
		return _metaDataExport;
	}

	//
	// export meta data in given file
	//
	void ExportMetaData( std::string file_name );

	//
	// get the parameters required for Diamler
	//
	void GetAllParameters();
	// Set pitch angle of sensor.
	void SetTiltCorrectionPitch( float degree );

	// Set transport resolution of sensor.
	void SetTransportResolution( float transportResolution );

	void SetZmapResolution( float lateralResolution, float verticalResolution );
	void GetZmapResolution( float* lateralResolution, float* verticalResolution );

	float GetTransportResolution();

	void SetSmartXactMode( int mode );
	int  GetSmartXactMode();
};
