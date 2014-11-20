#include "Lock.h"

#include <Windows.h>

CRITICAL_SECTION globalCriticalSection;

Lock::Lock() { EnterCriticalSection(&globalCriticalSection); }
Lock::~Lock() { LeaveCriticalSection(&globalCriticalSection); }
