#pragma once
#include "SR_API_public.h"

#include <Windows.h>
#include <string.h>
#include <ctime>
#include <iostream>

//
// reflects the state of the sensor ip connection
//
enum SensorConnectionState
{
	SensorConnectionUnknown = 0,
	SensorConnecting        = 1,
	SensorConnected         = 2,
	SensorDisconnected      = 3
};


//
// represents the current state of a Sensor
//
struct SensorState
{
	// reflects the ip connection state of the sensor
	SensorConnectionState SensorConnection;

	std::string LastConnectionMessage;
	std::string LastInfoMessage;
	std::string LastErrorMessage;

	bool hasError;

	// increases for each package that have been received
	long ImagePackageCounter;

	// increases for each alive signal
	long AliveSignalCounter;


	//
	// Ctor
	//
	SensorState()
	{
		ImagePackageCounter = 0;
		AliveSignalCounter  = 0;
		SensorConnection    = SensorConnectionUnknown;

		hasError = false;
	}
};
