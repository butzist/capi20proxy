extern "C" {
	typedef DWORD	(*c_register)(DWORD,DWORD,DWORD,DWORD,DWORD*);
	typedef DWORD	(*c_release)(DWORD);
	typedef DWORD	(*c_put_message)(DWORD,LPVOID);
	typedef DWORD	(*c_get_message)(DWORD,LPVOID*);
	typedef DWORD	(*c_wait_for_signal)(DWORD);
	typedef void	(*c_get_manufacturer)(char*);
	typedef DWORD	(*c_get_version)(DWORD*,DWORD*,DWORD*,DWORD*);
	typedef DWORD	(*c_get_serial)(char*);
	typedef DWORD	(*c_get_profile)(LPVOID,DWORD);
    typedef DWORD	(*c_installed)(void);
}

#define MAX_CONTROLLERS		127
#define MAX_APPLICATIONS	256

#define AUTH_DESIRED		0

#define __PORT	6674
#define	_MSG_BUFFER	10000

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
		return sizeof(REQUEST_PROXY_SHUTDOWN);

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



void initControllerNum();
DWORD initConnection();
DWORD WINAPI messageDispatcher(LPVOID param);
DWORD APIENTRY CAPI_GET_PROFILE(LPVOID szBuffer, DWORD CtrlNr);
DWORD APIENTRY CAPI_RELEASE(DWORD ApplID);

