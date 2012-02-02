#ifndef _VP_H
#define _VP_H
void mutex_lock(CRITICAL_SECTION *SocketMutex)
{
	EnterCriticalSection(SocketMutex);
}

void MutexInit(CRITICAL_SECTION *SocketMutex ){
  InitializeCriticalSection(SocketMutex);
}

void MUtexUnInit(CRITICAL_SECTION *SocketMutex ) {
  DeleteCriticalSection(SocketMutex);
}
void mutex_unlock(CRITICAL_SECTION *SocketMutex)
{
	LeaveCriticalSection(SocketMutex);
}
#endif