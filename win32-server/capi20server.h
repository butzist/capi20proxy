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
 * Revision 1.6  2002/03/29 07:52:06  butzist
 * seems to work (got problems with DATA_B3)
 *
 * Revision 1.5  2002/03/22 16:48:06  butzist
 * just coded a little bit bt didn't finish
 *
 * Revision 1.4  2002/03/03 20:38:19  butzist
 * added Log history
 *
 */

/*
    Declarations for the main program
*/

#include "protocol.h"

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
DWORD WINAPI StartSession(LPVOID param);
int allocSession(SOCKET socke);
void freeSession(int sess);

#define APPL_ID(msg)	(*((UINT*)(((char*)msg)+2)))
#define CAPI_NCCI(msg)	(*((DWORD*)(((char*)msg)+8)))
#define CAPI_CMD1(msg)	(*((unsigned char*)(((char*)msg)+4)))
#define CAPI_CMD2(msg)	(*((unsigned char*)(((char*)msg)+5)))

void inline SET_CTRL(char* _msg, unsigned char _ctrl)
{
	DWORD& ncci=CAPI_NCCI(_msg);
	ncci &=  0xFFFFFF80;
	ncci += _ctrl;
}

struct TOParams{
	DWORD Milliseconds;
	int* var;
	int val;
};

extern SERVICE_STATUS          ServiceStatus; 
extern SERVICE_STATUS_HANDLE   ServiceStatusHandle; 
extern int run,pause; 
extern FILE *f;
extern HANDLE RunningThread;

inline LPVOID mymalloc(size_t _size)
{
	return HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,_size);
}

inline void myfree(LPVOID _ptr)
{
	HeapFree(GetProcessHeap(),0,_ptr);
}

int rbodysize(UINT type)
{
	switch(type)
	{
	case TYPE_CAPI_REGISTER:
		return sizeof(REQUEST_CAPI_REGISTER);

	case TYPE_CAPI_RELEASE:
		return sizeof(REQUEST_CAPI_RELEASE);

	case TYPE_CAPI_PUTMESSAGE:
		return sizeof(REQUEST_CAPI_PUTMESSAGE);

	case TYPE_CAPI_GETMESSAGE:
		return sizeof(REQUEST_CAPI_GETMESSAGE);

	case TYPE_CAPI_WAITFORSIGNAL:
		return sizeof(REQUEST_CAPI_WAITFORSIGNAL);

	case TYPE_CAPI_MANUFACTURER:
		return sizeof(REQUEST_CAPI_MANUFACTURER);

	case TYPE_CAPI_VERSION:
		return sizeof(REQUEST_CAPI_VERSION);

	case TYPE_CAPI_SERIAL:
		return sizeof(REQUEST_CAPI_SERIAL);

	case TYPE_CAPI_PROFILE:
		return sizeof(REQUEST_CAPI_PROFILE);

	case TYPE_CAPI_INSTALLED:
		return sizeof(REQUEST_CAPI_INSTALLED);

	case TYPE_PROXY_HELO:
		return sizeof(REQUEST_PROXY_HELO);

	case TYPE_PROXY_AUTH:
		return sizeof(REQUEST_PROXY_AUTH);

	case TYPE_PROXY_KEEPALIVE:
		return sizeof(REQUEST_PROXY_KEEPALIVE);

	case TYPE_PROXY_SHUTDOWN:
		return 0;

	default:
		return -1;
	}
}

int abodysize(UINT type)
{
	switch(type)
	{
	case TYPE_CAPI_REGISTER:
		return sizeof(ANSWER_CAPI_REGISTER);

	case TYPE_CAPI_RELEASE:
		return sizeof(ANSWER_CAPI_RELEASE);

	case TYPE_CAPI_PUTMESSAGE:
		return sizeof(ANSWER_CAPI_PUTMESSAGE);

	case TYPE_CAPI_GETMESSAGE:
		return sizeof(ANSWER_CAPI_GETMESSAGE);

	case TYPE_CAPI_WAITFORSIGNAL:
		return sizeof(ANSWER_CAPI_WAITFORSIGNAL);

	case TYPE_CAPI_MANUFACTURER:
		return sizeof(ANSWER_CAPI_MANUFACTURER);

	case TYPE_CAPI_VERSION:
		return sizeof(ANSWER_CAPI_VERSION);

	case TYPE_CAPI_SERIAL:
		return sizeof(ANSWER_CAPI_SERIAL);

	case TYPE_CAPI_PROFILE:
		return sizeof(ANSWER_CAPI_PROFILE);

	case TYPE_CAPI_INSTALLED:
		return sizeof(ANSWER_CAPI_INSTALLED);

	case TYPE_PROXY_HELO:
		return sizeof(ANSWER_PROXY_HELO);

	case TYPE_PROXY_AUTH:
		return sizeof(ANSWER_PROXY_AUTH);

	case TYPE_PROXY_KEEPALIVE:
		return sizeof(ANSWER_PROXY_KEEPALIVE);

	case TYPE_PROXY_SHUTDOWN:
		return sizeof(ANSWER_PROXY_SHUTDOWN);

	default:
		return -1;
	}
}

struct appl_list
{
	appl_list* next;
	DWORD ApplID;
};

struct waiter_data
{
	SOCKET socket;
	DWORD ApplID;
	DWORD message_id;
	DWORD session_id;
};

struct client_data
{
	SOCKET socket;
	DWORD auth_type;
	LPVOID auth_data;
	char name[64];
	__version_t	version;
	DWORD keepalive;
	DWORD session;
	int os;
	appl_list* registered_apps;
};
