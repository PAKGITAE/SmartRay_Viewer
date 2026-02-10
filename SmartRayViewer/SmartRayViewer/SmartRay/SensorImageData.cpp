#include "SensorImageData.h"

#include <iostream>

SensorImageData::SensorImageData()
    : Width( 0 )
    , Height( 0 )
    , CurrentHeight( 0 )
    , ImageType( ImageDataType_Invalid )
    , OriginX( 0 )
{}

SensorImageData::SensorImageData( int           width,
                                  int           height,
                                  ImageDataType imageType,
                                  int           originX,
                                  float         originYMillimeters,
                                  int           currentHeight )
    : Width( width )
    , Height( height )
    , CurrentHeight( currentHeight )
    , ImageType( imageType )
    , OriginX( originX )
    , OriginYMillimeters( originYMillimeters )
{}

SensorImageData::SensorImageData( SensorImageData const& src )
    : Width( src.Width )
    , Height( src.Height )
    , CurrentHeight( src.CurrentHeight )
    , ImageType( src.ImageType )
    , OriginX( src.OriginX )
    , OriginYMillimeters( src.OriginYMillimeters )
    , LiveImage( src.LiveImage )
    , ProfileImage( src.ProfileImage )
    , IntensityImage( src.IntensityImage )
    , LaserLineThicknessImage( src.LaserLineThicknessImage )
    , ZMapImage( src.ZMapImage )
    , ZMapIntensityImage( src.ZMapIntensityImage )
    , ZMapLaserLineThicknessImage( src.ZMapLaserLineThicknessImage )
{}

SensorImageData& SensorImageData::operator=( SensorImageData const& src )
{
	Width                       = src.Width;
	Height                      = src.Height;
	CurrentHeight               = src.CurrentHeight;
	ImageType                   = src.ImageType;
	OriginX                     = src.OriginX;
	OriginYMillimeters          = src.OriginYMillimeters;
	LiveImage                   = src.LiveImage;
	ProfileImage                = src.ProfileImage;
	IntensityImage              = src.IntensityImage;
	LaserLineThicknessImage     = src.LaserLineThicknessImage;
	ZMapImage                   = src.ZMapImage;
	ZMapIntensityImage          = src.ZMapIntensityImage;
	ZMapLaserLineThicknessImage = src.ZMapLaserLineThicknessImage;
	return *this;
}

bool SensorImageData::ContainsFullImages()
{
	int fullSize = Width * Height;

	switch( ImageType )
	{
		case ImageDataType_LiveImage:
			return ( LiveImage.GetImageSize() == fullSize );

		case ImageDataType_Profile:
			return ( ProfileImage.GetImageSize() == fullSize );

		case ImageDataType_Intensity:
			return ( IntensityImage.GetImageSize() == fullSize );

		case ImageDataType_ProfileIntensity:
			return ( ProfileImage.GetImageSize() == fullSize && IntensityImage.GetImageSize() == fullSize );

		case ImageDataType_ProfileIntensityLaserLineThickness:
			return ( ProfileImage.GetImageSize() == fullSize && IntensityImage.GetImageSize() == fullSize
			         && LaserLineThicknessImage.GetImageSize() == fullSize );

		case ImageDataType_ZMap:
			return ( ZMapImage.GetImageSize() == fullSize );

		case ImageDataType_ZMapIntensity:
			return ( ZMapImage.GetImageSize() == fullSize && ZMapIntensityImage.GetImageSize() == fullSize );

		case ImageDataType_ZMapIntensityLaserLineThickness:
			return ( ZMapImage.GetImageSize() == fullSize && ZMapIntensityImage.GetImageSize() == fullSize
			         && ZMapLaserLineThicknessImage.GetImageSize() == fullSize );

		default:
			return false;
	}

	return false;
}


void SensorImageData::SaveImage( std::string filename, PackagedImage<uint16_t>& imageData )
{
	if( imageData.GetImageSize() == 0 )
		return;

	PackagedImage<uint16_t>::Package image = imageData.GetFullImage();

	cv::Mat mat;
	mat.create( CurrentHeight, Width, CV_16UC1 );
	memcpy( mat.data, image.data(), CurrentHeight * Width * 2 );
	cv::imwrite( filename.c_str(), mat );

	mat.release();
}

void SensorImageData::SaveImage( std::string filename, PackagedImage<uint8_t>& imageData )
{
	if( imageData.GetImageSize() == 0 )
		return;

	PackagedImage<uint8_t>::Package image = imageData.GetFullImage();

	cv::Mat mat;
	mat.create( CurrentHeight, Width, CV_8UC1 );
	memcpy( mat.data, image.data(), CurrentHeight * Width * 1 );
	cv::imwrite( filename.c_str(), mat );

	mat.release();
}


void SensorImageData::SaveLiveImage( std::string filename )
{
	std::cout << "saving live image: " << filename.c_str() << std::endl;
	SaveImage( filename + ".png", LiveImage );
}

void SensorImageData::SavePilImage( std::string filename )
{
	std::cout << "saving PIL image: " << filename.c_str() << std::endl;
	SaveImage( filename + "_P.png", ProfileImage );
	SaveImage( filename + "_I.png", IntensityImage );
	SaveImage( filename + "_L.png", LaserLineThicknessImage );
}

void SensorImageData::SaveZilImage( std::string filename )
{
	std::cout << "saving ZIL image: " << filename.c_str() << std::endl;
	SaveImage( filename + "_Z.png", ZMapImage );
	SaveImage( filename + "_I.png", ZMapIntensityImage );
	SaveImage( filename + "_L.png", ZMapLaserLineThicknessImage );
}

cv::Mat SensorImageData::GetLiveImage()
{
	cv::Mat mat;
	mat.create( CurrentHeight, Width, CV_8UC1 );
	PackagedImage<uint8_t>::Package image = LiveImage.GetFullImage();
	memcpy( mat.data, image.data(), CurrentHeight * Width * 1 );

	return mat;
}

cv::Mat SensorImageData::GetProfileImage()
{
	cv::Mat mat;
	mat.create( CurrentHeight, Width, CV_16UC1 );
	PackagedImage<uint16_t>::Package image = ProfileImage.GetFullImage();
	memcpy( mat.data, image.data(), CurrentHeight * Width * 2 );

	return mat;
}
