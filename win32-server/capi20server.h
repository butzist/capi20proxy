/*
 *   capi20proxy win32 server (Provides a remote CAPI port over TCP/IP)
 *   Copyright (C) 2002  Adam Szalkowski <adam@szalkowski.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * $Log$
 */

/*
    Declarations for the main program
*/

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
