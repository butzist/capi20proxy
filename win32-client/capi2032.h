/*
 *   capi20proxy win32 client (Provides a remote CAPI port over TCP/IP)
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
    main header for capi2032.dll
*/

#define AUTH_DESIRED		0

#define __PORT	6674
#define	_MSG_BUFFER	10000

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

struct applmap
{
	UINT	local;
	UINT	remote;
};

struct evlhash
{
	evlhash* next;

	DWORD msgId;
	char** data;
	HANDLE thread;
};

inline LPVOID malloc(size_t _size)
{
	return HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,_size);
}

inline void free(LPVOID _ptr)
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
	void* data;
};


DWORD initConnection();
DWORD shutdownConnection();
DWORD WINAPI messageDispatcher(LPVOID param);

DWORD APIENTRY CAPI_REGISTER(DWORD MessageBufferSize, DWORD maxLogicalConnection, DWORD maxBDataBlocks, DWORD maxBDataLen, DWORD *pApplID);
DWORD APIENTRY CAPI_RELEASE(DWORD ApplID);
DWORD APIENTRY CAPI_PUT_MESSAGE(DWORD ApplID, LPVOID pCAPIMessage);
DWORD APIENTRY CAPI_GET_MESSAGE(DWORD ApplID, LPVOID *ppCAPIMessage);
DWORD APIENTRY CAPI_WAIT_FOR_SIGNAL(DWORD ApplID);
void APIENTRY CAPI_GET_MANUFACTURER(char* szBuffer);
DWORD APIENTRY CAPI_GET_VERSION(DWORD * pCAPIMajor, DWORD * pCAPIMinor, DWORD * pManufacturerMajor, DWORD * pManufacturerMinor);
DWORD APIENTRY CAPI_GET_SERIAL_NUMBER(char* szBuffer);
DWORD APIENTRY CAPI_GET_PROFILE(LPVOID szBuffer, DWORD CtrlNr);
DWORD APIENTRY CAPI_INSTALLED(void);

void add_app(DWORD _ApplID, appl_list** _list);
void del_app(DWORD _ApplID, appl_list** _list);
DWORD getfirst_app(appl_list** _list);
void* getdata_app(DWORD _ApplID, appl_list* list);
void setdata_app(DWORD _ApplID, appl_list* list, void* _data);
