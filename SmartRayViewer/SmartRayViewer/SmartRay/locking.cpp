
#include "locking.h"

BaseLock::BaseLock()
{}

BaseLock::~BaseLock()
{}

//
// CriticalSectionEx class
//

CriticalSectionEx::CriticalSectionEx()
    : BaseLock()
{
	InitializeCriticalSection( (CRITICAL_SECTION*)&m_criticalSection );
	m_lockCounter = 0;
}

CriticalSectionEx::CriticalSectionEx( const CriticalSectionEx& b )
    : BaseLock()
{
	InitializeCriticalSection( (CRITICAL_SECTION*)&m_criticalSection );
	m_lockCounter = 0;
}

CriticalSectionEx::~CriticalSectionEx()
{
	// assert(m_lockCounter==0);
	DeleteCriticalSection( (CRITICAL_SECTION*)&m_criticalSection );
}

bool CriticalSectionEx::Lock() const
{
	// cast away const is needed, due to missing mutable keyword previous to C++11
	EnterCriticalSection( (CRITICAL_SECTION*)&m_criticalSection );
	( *( (int*)&m_lockCounter ) )++;
	return true;
}
long CriticalSectionEx::TryLock() const
{
	long ret = TryEnterCriticalSection( (CRITICAL_SECTION*)&m_criticalSection );
	if( ret )
	{
		( *( (int*)&m_lockCounter ) )++;
	}
	return ret;
}

long CriticalSectionEx::TryLockFor( const unsigned int dwMilliSecond ) const
{
	long ret = 0;
	if( ret = TryEnterCriticalSection( (CRITICAL_SECTION*)&m_criticalSection ) )
	{
		( *( (int*)&m_lockCounter ) )++;
		return ret;
	}
	ULONGLONG startTime, timeUsed;
	ULONGLONG waitTime = dwMilliSecond;
	startTime          = GetTickCount64();
	while( WaitForSingleObject( m_criticalSection.LockSemaphore, dwMilliSecond ) == WAIT_OBJECT_0 )
	{
		// cast away const is needed, due to missing mutable keyword previous to C++11
		if( ret = TryEnterCriticalSection( (CRITICAL_SECTION*)&m_criticalSection ) )
		{
			( *( (int*)&m_lockCounter ) )++;
			return ret;
		}
		timeUsed  = GetTickCount64() - startTime;
		waitTime  = waitTime - timeUsed;
		startTime = GetTickCount();
	}
	return 0;
}

void CriticalSectionEx::Unlock() const
{
	//!!!assert(m_lockCounter>=0);
	// cast away const is needed, due to missing mutable keyword previous to C++11
	LeaveCriticalSection( (CRITICAL_SECTION*)&m_criticalSection );
}
