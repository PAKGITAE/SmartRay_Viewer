#pragma once
#include <stdint.h>
#include <vector>
#include "SR_API_public.h"
#include "opencv2\opencv.hpp"

#include "PackagedImage.h"

//
// holds the image data as reported by the sensor
//
class SensorImageData
{
public:
	int           Width;
	int           Height;
	int           OriginX;
	float         OriginYMillimeters;
	ImageDataType ImageType;

	// current height of the image
	int CurrentHeight;

	// live image component
	PackagedImage<uint8_t> LiveImage;

	// PIL image components
	PackagedImage<uint16_t> ProfileImage;
	PackagedImage<uint16_t> IntensityImage;
	PackagedImage<uint16_t> LaserLineThicknessImage;

	// ZIL image components
	PackagedImage<uint16_t> ZMapImage;
	PackagedImage<uint16_t> ZMapIntensityImage;
	PackagedImage<uint16_t> ZMapLaserLineThicknessImage;

private:
	//
	// write a 16 bit image. The format is defined by the file ending
	//
	void SaveImage( std::string filename, PackagedImage<uint16_t>& image );

	//
	// write a 8 bit image. The format is defined by the file ending
	//
	void SaveImage( std::string filename, PackagedImage<uint8_t>& image );


public:
	//
	// default Ctor
	//
	SensorImageData();

	//
	// Ctor
	//
	SensorImageData( int           width,
	                 int           fullHeight,
	                 ImageDataType imageType,
	                 int           originX            = 0,
	                 float         originYMillimeters = 0,
	                 int           currentHeight      = 0 );

	//
	// copy Ctor
	//
	SensorImageData( SensorImageData const& src );
	SensorImageData& operator=( SensorImageData const& src );

	//
	// returns true when at least one valid image is contained matching the height
	//
	bool ContainsFullImages();


	//
	// save live image data
	//
	void SaveLiveImage( std::string filename );

	//
	// save PIL image data
	//
	void SavePilImage( std::string filename );

	//
	// save ZIL image data
	//
	void SaveZilImage( std::string filename );

	//
	// get cvMat of live image
	//
	cv::Mat GetLiveImage();

	//
	// get cvMat of profile image (PIL)
	//
	cv::Mat GetProfileImage();
};
