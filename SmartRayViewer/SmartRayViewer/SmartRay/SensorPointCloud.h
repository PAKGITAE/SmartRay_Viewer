#pragma once

#include "PackagedImage.h"
#include "SR_API_Defines.h"

#include <fstream>
#include <iomanip>

class PointCloud
{
protected:
	std::vector<SR_3DPOINT> _points;
	std::vector<uint16_t>   _intensity;
	std::vector<uint16_t>   _laserLineThickness;
	std::vector<uint32_t>   _profileIdx;
	std::vector<uint32_t>   _columnIdx;

public:
	PointCloud()
	{}

	PointCloud( uint32_t    numPoints,
	            SR_3DPOINT* point_cloud,
	            uint16_t*   intensity,
	            uint16_t*   laserlinethickness,
	            uint32_t*   profileIdx,
	            uint32_t*   columnIdx )
	{
		_points             = make_vector( point_cloud, numPoints );
		_intensity          = make_vector( intensity, numPoints );
		_laserLineThickness = make_vector( laserlinethickness, numPoints );
		_profileIdx         = make_vector( profileIdx, numPoints );
		_columnIdx          = make_vector( columnIdx, numPoints );
		std::cout << "--- Point Cloud data created (#points: " << numPoints << ").\n";
	}

	static void PrintHeader( const std::string& filename, const float transportResolution )
	{
		std::ofstream ofile( filename );
		if( ofile.good() )
		{
			ofile << std::endl;
			ofile << "NOTE: transportRes = " << transportResolution << std::endl;
			ofile << std::endl;

			ofile << std::setw( 10 ) << std::left << "id";
			ofile << std::setw( 12 ) << std::left << "p.x";
			ofile << std::setw( 12 ) << std::left << "p.y";
			ofile << std::setw( 12 ) << std::left << "p.z";
			ofile << std::setw( 12 ) << std::left << "Profile";
			ofile << std::setw( 10 ) << std::left << "Column";
			ofile << std::setw( 10 ) << std::left << "Intens.";
			ofile << std::setw( 10 ) << std::left << "LLT";
			ofile << std::endl;
		}
		ofile.close();
	}

	static uint32_t PrintPoints( const std::string&             filename,
	                             const bool                     saveAllPoints,
	                             uint32_t                       startPointIdx,
	                             std::vector<SR_3DPOINT> const& points,
	                             std::vector<uint16_t> const&   intensity,
	                             std::vector<uint16_t> const&   laserLineThickness,
	                             std::vector<uint32_t> const&   profileIdx,
	                             std::vector<uint32_t> const&   columnIdx )
	{
		std::cout << "--- Saving Point Cloud data to file...";

		uint32_t validPointIdx = startPointIdx;  // Valid points counter.

		std::ofstream ofile;
		ofile.open( filename, std::ios_base::app );  // Open file for append.
		if( !ofile.good() )
		{
			std::cout << " Failed!" << std::endl;
			return validPointIdx;
		}

		ofile.precision( 3 );
		ofile.setf( std::ios::fixed, std::ios::floatfield );

		for( uint32_t i = 0; i < points.size(); ++i )
		{
			// Skip invalid points (unless printing of all data points is requested):
			SR_3DPOINT const& point = points[i];
			if( point.x <= NONWORLDMARK && !saveAllPoints )
			{
				continue;
			}
			validPointIdx++;

			// Print data into file:
			ofile << std::setw( 10 ) << std::left << validPointIdx;
			ofile << std::setw( 12 ) << std::left << point.x;
			ofile << std::setw( 12 ) << std::left << point.y;
			ofile << std::setw( 12 ) << std::left << point.z;
			if( !profileIdx.empty() )
				ofile << std::setw( 12 ) << std::left << profileIdx[i];
			if( !columnIdx.empty() )
				ofile << std::setw( 10 ) << std::left << columnIdx[i];
			if( !intensity.empty() )
				ofile << std::setw( 10 ) << std::left << intensity[i];
			if( !laserLineThickness.empty() )
				ofile << std::setw( 10 ) << std::left << laserLineThickness[i];
			ofile << std::endl;
		}  // for i
		ofile.close();
		std::cout << " Done!" << std::endl;
		return validPointIdx;
	}

	uint32_t SavePointCloud( const std::string& filename, uint32_t startPointIdx, const bool saveAllPoints = false )
	{
		return PointCloud::PrintPoints(
		    filename, saveAllPoints, startPointIdx, _points, _intensity, _laserLineThickness, _profileIdx, _columnIdx );
	}
};
