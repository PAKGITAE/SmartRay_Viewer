#pragma once
#include "SR_API_Defines.h"
#include "SR_API_public.h"
#include "sr_api_errorcodes.h"

#include "ApiState.h"
#include "Sensor.h"
#include "SensorImageData.h"
#include "locking.h"

#include <Windows.h>
#include <stdint.h>
#include <string.h>
#include <ctime>
#include <iostream>
#include <vector>

//
// sample class for Smartray API access
//
class SensorManager
{
	friend class Sensor;

protected:
	// list of managed sensors
	static std::vector<SharedPtr<Sensor>> _sensors;

private:
	//
	// message and status callback, called by the api
	//
	static int ApiStatusCallback( SRSensor*      sensor,
	                              MessageType    msgType,
	                              SubMessageType subMsgType,
	                              int            msgData,
	                              char*          msg );

	//
	// default callback when a live image is retrieved
	// 8 Bit grayscale
	//
	static int LiveImageCallback( SRSensor*     sensor,
	                              ImageDataType imageType,
	                              int           originX,
	                              int           height,
	                              int           width,
	                              uint8_t*      liveImage );

	//
	// default callback when a PIL image is retrieved
	// 16 Bit image data (profile image, intensitiy image, laser line thickness
	// image)
	//
	static int PilImageCallback( SRSensor*     sensor,
	                             ImageDataType imageType,
	                             int           originX,
	                             int           height,
	                             int           width,
	                             uint16_t*     profileImage,
	                             uint16_t*     intensityImage,
	                             uint16_t*     lltImage,
	                             int           numExtData,
	                             void*         extData );

	//
	// default callback when a ZIL (Zmap) image is retrieved
	// 16 Bit image data (profile zmap image, intensitiy zmap image, laser line
	// thickness zmap image)
	//
	static int ZilImageCallback( SRSensor*     sensor,
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
	                             void*         extData );

	//
	// Default callback when a Point Cloud is retrieved.
	// Parameters: Pointers to Point Cloud and data associated to each point: intensity, laser line thickness,
	// sensorIdx, profileIdx, columnIdx.
	//
	static int PointCloudCallback( SRSensor*     sensor,
	                               ImageDataType dattyp,
	                               uint32_t      numPoints,
	                               uint32_t      numProfile,
	                               SR_3DPOINT*   point_cloud,
	                               uint16_t*     intensity,
	                               uint16_t*     laserlinethickness,
	                               uint32_t*     profileIdx,
	                               uint32_t*     columnIdx,
	                               uint32_t      numExtData,
	                               void*         extData );

	//
	// default callback when an MSR point cloud  is retrieved
	// Pointers to point cloud and data associated to each point: intensity, laser line thickness, sensorIdx,
	// profileIdx, pointIdx
	//
	static int MSRPointCloudCallback( SRSensor*   md,
	                                  uint32_t    dattyp,
	                                  uint32_t    numPoints,
	                                  SR_3DPOINT* point_cloud,
	                                  uint16_t*   intensity,
	                                  uint16_t*   laserlinethickness,
	                                  uint32_t*   sensorIdx,
	                                  uint32_t*   profileIdx,
	                                  uint32_t*   pointIdx,
	                                  uint32_t    numMSRExtData,
	                                  void*       extMSRData );

	//
	// handle error codes after calling an API function
	// returns true when the api function succeeded
	//
	static bool HandleReturnCode( int apiReturnCode, bool fatal );

	//
	// handle error codes after calling an API function
	// all failures are treated as fatal errors which causes a termination
	//
	static bool HandleReturnCode( int apiReturnCode );

	//
	// handle & report errors
	// when fatal is true, the application is terminated
	//
	static void HandleError( std::string text, bool fatal = true );

	//
	// try to get a sensor by it's internal sensor descriptor object
	// otherwise NULL is returned
	//
	static SharedPtr<Sensor> TryGetSensor( SRSensor* sensorObject );

	static void AddMetaData( Sensor& sensor, uint32_t numberElements, size_t sizeExtDataEntry, void* extData );

protected:
	//
	// inits the Smartray API
	//
	void InitApi( bool useMSRPointCloudCB );

	//
	// teardown the Smartray API
	//
	void DeinitApi();

public:
	//
	// Ctor
	//
	SensorManager( bool useMSRPointCloudCB = false );

	//
	// Dtor
	//
	~SensorManager();

	//
	// create sensor object
	//
	SharedPtr<Sensor> CreateSensor( std::string    name,
	                                const char*    ipAddress = DEFAULT_IP_ADR,
	                                unsigned short port      = DEFAULT_PORT_NUM );
};
