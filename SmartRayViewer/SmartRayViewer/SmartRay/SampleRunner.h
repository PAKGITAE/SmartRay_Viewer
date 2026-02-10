#pragma once
#include <string.h>
#include <iostream>

typedef void ( *SampleMethod )();


//
// thrown when a fatal error occured
//
class FatalErrorException : public std::exception
{
public:
	//
	// ctor
	//
	FatalErrorException( std::string message )
	    : std::exception( message.c_str() )
	{}
};


//
// runs a sample method and handles errors
//
class SampleRunner
{
private:
	const SampleMethod _method;

public:
	const std::string Description;
	const bool        AutoRun;

	//
	// Ctor
	//
	SampleRunner( std::string description, SampleMethod method, bool autorun = false )
	    : Description( description )
	    , _method( method )
	    , AutoRun( autorun )
	{}

	//
	// run the sample
	//
	bool Run()
	{
		try
		{
			std::cout << "executing sample '" << Description.c_str() << "'" << std::endl << std::endl;
			_method();
			return true;
		}
		catch( FatalErrorException e )
		{
			std::cout << e.what() << std::endl;
			return false;
		}
		catch( std::exception e )
		{
			std::cout << "an exception occured during the execution of sample '" << Description.c_str() << "'"
			          << std::endl;
			return false;
		}
	}
};
