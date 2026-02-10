#pragma once

#include "PackagedImage.h"

#include <fstream>
#include <vector>

struct SensorPointcloudMSR
{
	std::vector<SR_3DPOINT> _points;
	std::vector<uint16_t>   _laserlinethickness;
	std::vector<uint16_t>   _intensity;
	std::vector<uint32_t>   _sensorIdx;
	std::vector<uint32_t>   _profileIdx;
	std::vector<uint32_t>   _pointIdx;

	SensorPointcloudMSR()
	{}

	SensorPointcloudMSR( uint32_t    numPoints,
	                     SR_3DPOINT* point_cloud,
	                     void*       intensity,
	                     void*       laserlinethickness,
	                     uint32_t*   sensorIdx,
	                     uint32_t*   profileIdx,
	                     uint32_t*   pointIdx )
	{
		_points = make_vector( point_cloud, numPoints );

		if( intensity )
		{
			_intensity = make_vector( static_cast<uint16_t*>( intensity ), numPoints );
		}

		if( laserlinethickness )
		{
			_laserlinethickness = make_vector( static_cast<uint16_t*>( laserlinethickness ), numPoints );
		}

		if( sensorIdx && profileIdx && pointIdx )
		{
			_sensorIdx  = make_vector( static_cast<uint32_t*>( sensorIdx ), numPoints );
			_profileIdx = make_vector( static_cast<uint32_t*>( profileIdx ), numPoints );
			_pointIdx   = make_vector( static_cast<uint32_t*>( pointIdx ), numPoints );
		}
	}

	void SavePointCloud( const std::string& filename )
	{
		std::ofstream ofile( filename );
		if( !ofile.good() )
		{
			return;
		}
		ofile.precision( 3 );
		ofile.setf( std::ios::fixed, std::ios::floatfield );
		ofile << "Number of points:\t" << _points.size() << std::endl;
		ofile << "idx\tp.x\tp.y\tp.z\t";
		if( !_intensity.empty() )
		{
			ofile << "int\t";
		}
		if( !_laserlinethickness.empty() )
		{
			ofile << "llt\t";
		}
		if( !_sensorIdx.empty() )
		{
			ofile << "idxSen\tidxProf\tidxPoint" << std::endl;
		}

		for( size_t i = 0; i < _points.size(); ++i )
		{
			ofile << i << "\t" << _points[i].x << "\t" << _points[i].y << "\t" << _points[i].z << "\t";
			if( !_intensity.empty() )
			{
				ofile << _intensity[i] << "\t";
			}
			if( !_laserlinethickness.empty() )
			{
				ofile << _laserlinethickness[i] << "\t";
			}
			if( !_sensorIdx.empty() )
			{
				ofile << _sensorIdx[i] << "\t" << _profileIdx[i] << "\t" << _pointIdx[i] << "\t";
			}
			ofile << std::endl;
		}
	}
};
