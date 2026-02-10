#pragma once


#include <Windows.h>


// defines the default locking mechanism
#define DefaultLock CriticalSectionEx

//
// abstract BaseLock class
//
class BaseLock
{
public:
	BaseLock();
	virtual ~BaseLock();
	virtual bool Lock() const                                         = 0;
	virtual long TryLock() const                                      = 0;
	virtual long TryLockFor( const unsigned int dwMilliSecond ) const = 0;
	virtual void Unlock() const                                       = 0;
};

//
// CriticalSectionEx class
//
class CriticalSectionEx : public BaseLock
{
	CRITICAL_SECTION m_criticalSection;
	int              m_lockCounter;

public:
	CriticalSectionEx();
	CriticalSectionEx( const CriticalSectionEx& b );
	virtual ~CriticalSectionEx();
	CriticalSectionEx& operator=( const CriticalSectionEx& b )
	{
		return *this;
	}
	virtual bool Lock() const;
	virtual long TryLock() const;
	virtual long TryLockFor( const unsigned int dwMilliSecond ) const;
	virtual void Unlock() const;
};

class ScopedLock
{
public:
	ScopedLock( CriticalSectionEx const& mutex )
	    : m_mutex( mutex )
	{
		m_mutex.Lock();
	}
	~ScopedLock()
	{
		m_mutex.Unlock();
	}

private:
	CriticalSectionEx const& m_mutex;
};

// TODO replace with std::shared_ptr asap (C++11)
template<typename T>
class SharedPtr
{
public:
	SharedPtr( T* data = nullptr )
	    : m_RefCounter( nullptr )
	    , m_Data( data )
	{
		if( m_Data )
		{
			m_RefCounter = new int( 1 );
		}
	}
	SharedPtr( SharedPtr<T> const& ptr )
	    : m_RefCounter( ptr.m_RefCounter )
	    , m_Data( ptr.m_Data )
	{
		IncRefCounter();
	}

	~SharedPtr()
	{
		if( m_Data )
		{
			DecRefCounter();
		}
	}

	T* get()
	{
		return m_Data;
	}

	T const* get() const
	{
		return m_Data;
	}

	SharedPtr<T>& operator=( SharedPtr<T> const& ptr )
	{
		if( m_Data )
		{
			DecRefCounter();
		}
		m_RefCounter = ptr.m_RefCounter;
		m_Data       = ptr.m_Data;
		IncRefCounter();
		return *this;
	}

	T& operator*()
	{
		return *m_Data;
	}

	T const& operator*() const
	{
		return *m_Data;
	}

	T* operator->()
	{
		return m_Data;
	}

	T const* operator->() const
	{
		return m_Data;
	}

	operator bool() const
	{
		return m_Data != nullptr;
	}

private:
	void IncRefCounter() const
	{
		// const_cast
		++( *( (int*)m_RefCounter ) );
	}
	void DecRefCounter() const
	{
		// const_cast
		--( *( (int*)m_RefCounter ) );
		if( 0 == ( *m_RefCounter ) )
		{
			delete m_Data;
			delete m_RefCounter;
		}
		// const_cast
		( *( (T**)&m_Data ) ) = nullptr;
		// const_cast
		( *( (int**)&m_RefCounter ) ) = nullptr;
	}

	int* m_RefCounter;
	T*   m_Data;
};
