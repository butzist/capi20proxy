// CAPI functions
extern "C" {
extern DWORD APIENTRY CAPI_REGISTER (DWORD MessageBufferSize, DWORD maxLogicalConnection, DWORD maxBDataBlocks, DWORD maxBDataLen, DWORD *pApplID);
extern DWORD APIENTRY CAPI_RELEASE (DWORD ApplID);
extern DWORD APIENTRY CAPI_PUT_MESSAGE (DWORD ApplID, PVOID pCAPIMessage);
extern DWORD APIENTRY CAPI_GET_MESSAGE (DWORD ApplID, PVOID *ppCAPIMessage);
extern DWORD APIENTRY CAPI_WAIT_FOR_SIGNAL (DWORD ApplID);
extern DWORD APIENTRY CAPI_GET_MANUFACTURER (PVOID SzBuffer);
extern DWORD APIENTRY CAPI_GET_VERSION (DWORD *pCAPIMajor, DWORD *pCAPIMinor, DWORD *pManufacturerMajor, DWORD *pManufacturerMinor);
extern DWORD APIENTRY CAPI_GET_SERIAL_NUMBER (PVOID SzBuffer);
extern DWORD APIENTRY CAPI_GET_PROFILE (PVOID SzBuffer, DWORD CtrlNr);
extern DWORD APIENTRY CAPI_INSTALLED (void);
}

void DebugOut(char* msg,BOOL type_error=TRUE);
void WINAPI ServiceStart (DWORD argc, LPTSTR *argv);
DWORD WINAPI ReceiveRequest(LPVOID param);
DWORD ServiceInitialization(DWORD argc, LPTSTR *argv,DWORD *specificError); 
VOID WINAPI ServiceCtrlHandler (DWORD Opcode); 
DWORD WINAPI Wait(LPVOID params);
HANDLE TimeOut(DWORD Milliseconds,int* var,int val);
SOCKET CreateSocket(int socktype, int protocol, UINT port);
DWORD WINAPI TestStart(LPVOID param);
DWORD ServiceExit();

struct TOParams{
	DWORD Milliseconds;
	int* var;
	int val;
};

extern SERVICE_STATUS          ServiceStatus; 
extern SERVICE_STATUS_HANDLE   ServiceStatusHandle; 
extern int run,pause; 
extern FILE *f;
extern SOCKET socke;
extern HANDLE RunningThread;
