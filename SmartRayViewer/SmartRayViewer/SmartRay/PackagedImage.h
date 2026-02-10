
#pragma once

#include "locking.h"

#include <stdint.h>
#include <string>

template<typename DataT>
std::vector<DataT> make_vector( DataT* data, int size )
{
	if( !data )
	{
		return std::vector<DataT>();
	}
	std::vector<DataT> ret( size );
	memcpy( ret.data(), data, size * sizeof( DataT ) );
	return ret;
}

//
// contains sensor image data which is constructed out of certain profile packet
//
template<typename intType>
class PackagedImage
{
public:
	typedef std::vector<intType> Package;

	PackagedImage()
	{}
	//
	// copy ctor
	//
	PackagedImage( PackagedImage<intType> const& other )
	{
		ScopedLock guard( m_guard );

		_packages = other._packages;
	}

	PackagedImage& operator=( PackagedImage<intType> const& other )
	{
		ScopedLock guard( m_guard );

		_packages = other._packages;
		return *this;
	}

	//
	// Dtor
	//
	~PackagedImage()
	{
		ScopedLock guard( m_guard );

		_packages.clear();
	}

	//
	// append profile packet to the image
	//
	void AddPacket( intType* data, int size )
	{
		ScopedLock guard( m_guard );

		_packages.push_back( make_vector( data, size ) );
	}

	//
	// returns the full image as an array of the given value type
	//
	Package GetFullImage()
	{
		ScopedLock guard( m_guard );

		int size = GetImageSize_internal();
		if( size == 0 )
		{
			return Package();
		}

		Package image( size, 0 );
		int     imageIndex = 0;

		for( size_t i = 0; i < _packages.size(); ++i )
		{
			for( size_t j = 0; j < _packages[i].size(); j++ )
			{
				assert( "Image index out of range. GetImageSize does not match with total size of packages."
				        && static_cast<int>( imageIndex ) < image.size() );
				image[imageIndex++] = _packages[i][j];
			}
		}
		return image;
	}

	//
	// return the size the complete image of image elements
	//
	int GetImageSize()
	{
		ScopedLock guard( m_guard );

		return GetImageSize_internal();
	}

private:
	std::vector<Package> _packages;
	CriticalSectionEx    m_guard;

	// not thread safe
	int GetImageSize_internal()
	{
		size_t imageSize = 0;
		for( size_t i = 0; i < _packages.size(); ++i )
		{
			imageSize += _packages[i].size();
		}
		return static_cast<int>( imageSize );
	}
};
