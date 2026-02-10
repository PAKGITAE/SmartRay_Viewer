#pragma once
#include "SR_API_public.h"

#include <Windows.h>
#include <string.h>
#include <ctime>
#include <iostream>

// forward decl
enum SensorConnectionState;

//
// represents the current state of the API
//
struct ApiState
{
	SensorConnectionState SensorConnection;

	std::string LastConnectionMessage;
	std::string LastInfoMessage;
	std::string LastErrorMessage;

	long     LastSensorResponse;
	SRSensor LastSensorDescription;
};
