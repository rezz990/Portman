#ifndef PORTMAN_MAINTENANCE_H
#define PORTMAN_MAINTENANCE_H
/* Serialize app startup with install/uninstall file replacement for this session. */
static HANDLE pm_maintenance_enter(void) {
 HANDLE gate=CreateMutexW(NULL,FALSE,L"Local\\PortmanMaintenance02");
 if(!gate) return NULL;
 DWORD result=WaitForSingleObject(gate,0);
 if(result!=WAIT_OBJECT_0 && result!=WAIT_ABANDONED) { CloseHandle(gate); return NULL; }
 return gate;
}
static void pm_maintenance_leave(HANDLE gate) {
 if(gate) { ReleaseMutex(gate); CloseHandle(gate); }
}
#endif
