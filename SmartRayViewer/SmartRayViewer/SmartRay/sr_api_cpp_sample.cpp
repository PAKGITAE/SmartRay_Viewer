// sr_api_cpp_sample.cpp: main project file

#include "SR_API_public.h"

#include <Windows.h>
#include <string.h>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>

#include "SampleRunner.h"
#include "SensorManager.h"

#include "CppSamples.h"

//
// main
//
int main( int argc, char* argv[] )
{
	std::cout << "SmartRay API Samples" << std::endl;
	std::cout
	    << "!!! Ensure sensor is connected to the host computer as per the description in relevant user manual !!!"
	    << std::endl
	    << std::endl;
	std::vector<SampleRunner> samples;

	// register single sensor supported samples
	samples.push_back( SampleRunner( "Live Image Sample.", LiveImageSample, true ) );
	samples.push_back( SampleRunner( "PIL Image Sample.", PilImageSample, true ) );
	samples.push_back( SampleRunner( "Point Cloud Sample.", PointCloudSample, true ) );
	samples.push_back( SampleRunner( "Tilt Correction (Pitch Angle) Sample [Note: Applicable for ECCO 95/95+ series "
	                                 "only, especially for ECCO 95.XXXG sensors].",
	                                 TiltCorrectionSamplePitchAngle,
	                                 true ) );

	samples.push_back( SampleRunner( "ZIL Image Sample.", ZILImageSample, true ) );
	samples.push_back( SampleRunner( "Data Trigger Sample.", DataTriggerSample, true ) );
	samples.push_back( SampleRunner( "Start Trigger Sample [Note: Requires start trigger on input 1 of the sensor].",
	                                 StartTriggerSample ) );
	samples.push_back( SampleRunner( "Multiple Exposure Sample.", DoubleExposureSample, true ) );
	samples.push_back( SampleRunner( "Laser Brightness Sample.", LaserBrightnessSample, true ) );
	samples.push_back( SampleRunner( "Region Of Interest Sample.", RegionOfInterestSample, true ) );
	samples.push_back( SampleRunner( "Get Sensor Parameters Sample [Note: From Parameter Set i.e. *.par File].",
	                                 GetSensorParameters,
	                                 true ) );
	samples.push_back(
	    SampleRunner( "Metadata Sample [Note: Applicable for ECCO 95/95+ series only].", MetaDataSample, true ) );
	samples.push_back(
	    SampleRunner( "SmartXpress Sample [Note: Applicable for ECCO 95/95+ series only].", SmartXpressSample, true ) );
	samples.push_back(
	    SampleRunner( "SmartXact Sample [Note: Applicable for ECCO 95/95+ series only].", SmartXactSample, true ) );
	samples.push_back(
	    SampleRunner( "SmartXtract Sample [Note: Applicable for ECCO 95/95+ series only].", SmartXtractSample, true ) );
	samples.push_back( SampleRunner( "SmartXtract Archive Sample [Note: Applicable for ECCO 95/95+ series only].",
	                                 SmartXtractArchiveSample,
	                                 true ) );
	samples.push_back( SampleRunner( "SmartXtract with Multi-Exposure [Note: Applicable for ECCO 95/95+ series only].",
	                                 SmartXtractExposureModeSample,
	                                 true ) );

	// register multi sensor samples
	samples.push_back(
	    SampleRunner( "Multi-Sensor Sample (integrating more than one ECCO sensor using API).", MultiSensorSample ) );
	samples.push_back( SampleRunner(
	    "MSR Sample [Note: Applicable for ECCO 75/95 series - for specific models, refer MSR Application Note for "
	    "details. "
	    "Note that it is a prerequisite to have a valid registration file created using MSR Wizard (Studio 4)].",
	    MSRSample ) );
	samples.push_back(
	    SampleRunner( "Dual-Head ECCO 95+ Sample (BETA) [Note: Applicable for Dual-Head ECCO 95+ seris only. "
	                  "Note that it is a prerequisite to have a valid registration file created using Dual-Head "
	                  "Registration (Studio 4)].",
	                  DualHeadSample ) );

	// read command line arguments
	bool autorun = false;
	if( argc == 2 )
	{
		if( strcmp( argv[1], "/a" ) == 0 )
			autorun = true;
	}

	// autorun mode
	if( autorun )
	{
		// enumerate & display sample options
		int numberOfFailures = 0;
		int totalSamples     = 0;
		for( std::vector<SampleRunner>::iterator samplesIt = samples.begin(); samplesIt != samples.end(); samplesIt++ )
		{
			// skip non autorun samples
			if( !( *samplesIt ).AutoRun )
				continue;

			// run the sample
			totalSamples++;
			std::cout << "-------------------------------------------" << std::endl;
			if( !( *samplesIt ).Run() )
				numberOfFailures++;
		}
		std::cout << std::endl << "===========================================" << std::endl;
		std::cout << "--- number of failures / total runs: " << numberOfFailures << " / " << totalSamples << " ---"
		          << std::endl;
		if( numberOfFailures > 0 )
			return -1;

		return 0;
	}


	// interactive mode
	while( true )
	{
		// enumerate & display sample options
		for( size_t i = 0; i < samples.size(); ++i )
			std::cout << ( i + 1 ) << "\t" << samples[i].Description.c_str() << std::endl;

		// read user choice
		std::cout << "\nWhich sample to run? ";
		std::string input = "";
		std::getline( std::cin, input );
		if( input == "" )
			exit( 0 );
		try
		{
			int sampleNo = std::stoi( input );
			// run the sample
			if( 0 < sampleNo && sampleNo <= static_cast<int>( samples.size() ) )
			{
				samples[sampleNo - 1].Run();
			}
			else
			{
				std::cout << "Sample id (" << sampleNo << ") is not correct, please choose an id in range of [1, "
				          << samples.size() << "].\n";
			}
		}
		catch( std::invalid_argument& )
		{
			std::cout << "Invalid argument (" << input << ") for sample id, please choose an id in range of [1, "
			          << samples.size() << "].\n";
		}
		std::cout << std::endl << std::endl;
	}

	return 0;
}
