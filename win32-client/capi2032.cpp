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
 * Revision 1.17  2002/05/14 13:24:15  butzist
 * changed and recompiled
 * hope this works better
 *
 * Revision 1.16  2002/05/13 12:12:55  butzist
 * oh no, I can't think today... perhaps now it's right
 *
 * Revision 1.15  2002/05/13 11:58:32  butzist
 * maybe now it works :-)
 *
 * Revision 1.14  2002/05/13 11:17:26  butzist
 * found an error (used == instead of !=)
 * because of this incoming messages are deleted immediately
 *
 * Revision 1.13  2002/05/12 06:50:46  butzist
 * changed release version
 *
 * Revision 1.12  2002/05/12 06:24:06  butzist
 * added binaries and disabled timeout
 *
 * Revision 1.11  2002/04/11 09:02:07  butzist
 * found a bug (timeout)
 *
 * Revision 1.10  2002/04/09 14:32:00  butzist
 * works now quite fine
 * added timeout when receiving answer form server
 * handeled some more exceptions :-)
 *
 * Revision 1.9  2002/04/08 20:45:14  butzist
 * voice communication performance improved
 * global buffer now for each registered application
 * still hangs sometimes
 *
 * Revision 1.8  2002/03/29 22:13:50  butzist
 * added version information resource
 * made it finally work(!!!)
 * on telephony it lacks performance so you don't understand very much
 *
 * Revision 1.7  2002/03/29 07:52:32  butzist
 * seems to work (got problems with DATA_B3)
 *
 * Revision 1.6  2002/03/22 14:38:30  butzist
 * lol, I uploaded an old version yesterday. Now this is the new one.
 * At least it causes no compiler errors
 *
 * Revision 1.4  2002/03/03 20:38:19  butzist
 * added Log history
 *
 */

/*
    main source for capi2032.dll
*/

// capi2032.cpp : Definiert den Einsprungpunkt für die DLL-Anwendung.
//

#include "stdafx.h"
#include "protocol.h"
#include "capi2032.h"

#define VERSION_MAJOR	1
#define VERSION_MINOR	3

#define ILLEGAL_ANSWER		((char*)(void*)-1)
#define _WAITFORMESSAGE_TIMEOUT		0
IN_ADDR toaddr;

int status=0;
DWORD msg_id=0;
char* __myname[64];
SOCKET socke;
int max_timeout=-1, timeout=0;
HANDLE receiver;
evlhash* hash_start=NULL;
appl_list* registered_apps=NULL;
CRITICAL_SECTION hash_mutex, socket_mutex, dispatch_mutex, appllist_mutex;
UINT session_id=0;
DWORD _port;

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call) { 
		
	case DLL_PROCESS_ATTACH:
		WSADATA wsadata;
		if(WSAStartup(MAKEWORD(2,2),&wsadata)!=0)
			return FALSE;

		char ipstr[256];
		DWORD err,len;
		HKEY key;

		err=RegOpenKeyEx(HKEY_LOCAL_MACHINE,"Software\\The Red Guy\\capi20proxy",0,KEY_QUERY_VALUE,&key);
		if(err!=ERROR_SUCCESS){
			::MessageBox(NULL,"Could not open Registry Key \"HKEY_LOCAL_MACHINE\\Software\\The Red Guy\\capi20proxy\"","Error",MB_OK);
			return FALSE;
		}
		
		len=256;
		err=RegQueryValueEx(key,"server",NULL,NULL,(unsigned char*)ipstr,&len);
		if(err!=ERROR_SUCCESS){
			::MessageBox(NULL,"Could not read Regitry Value \"HKEY_LOCAL_MACHINE\\Software\\The Red Guy\\capi20proxy\\server\"","Error",MB_OK);
			return FALSE;
		}
		toaddr.S_un.S_addr=inet_addr(ipstr);

		len=64;
		err=RegQueryValueEx(key,"name",NULL,NULL,(unsigned char*)__myname,&len);
		if(err!=ERROR_SUCCESS){
			::MessageBox(NULL,"Could not read Regitry Value \"HKEY_LOCAL_MACHINE\\Software\\The Red Guy\\capi20proxy\\name\"","Error",MB_OK);
			return FALSE;
		}

		len=4;
		err=RegQueryValueEx(key,"port",NULL,NULL,(unsigned char*)&_port,&len);
		if(err!=ERROR_SUCCESS){
			_port=__PORT;
		}


		RegCloseKey(key);

		if(initConnection()!=NO_ERROR)
		{
			return FALSE;
		}

		InitializeCriticalSection(&hash_mutex);
		InitializeCriticalSection(&socket_mutex);
		InitializeCriticalSection(&dispatch_mutex);
		InitializeCriticalSection(&appllist_mutex);

		DWORD thid;
		receiver=CreateThread(NULL,1024,messageDispatcher,NULL,0,&thid);
		if(receiver==NULL)
		{
			return FALSE;
		}
		break;
 
	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:

		shutdown(socke,SD_RECEIVE);
		if(WaitForSingleObject(receiver,2000)!=WAIT_OBJECT_0)
		{
			closesocket(socke);
		} else {
			closesocket(socke);
	
			if(WaitForSingleObject(receiver,1000)!=WAIT_OBJECT_0)
			{
				TerminateThread(receiver,1);
			}
		}

		while(hash_start!=NULL)
		{
			*(hash_start->data)=ILLEGAL_ANSWER;
			hash_start=hash_start->next;
		}

		Sleep(500);

		while(getfirst_app(&registered_apps));		// free allocated memory in the list

		DeleteCriticalSection(&hash_mutex);
		DeleteCriticalSection(&socket_mutex);
		DeleteCriticalSection(&dispatch_mutex);
		DeleteCriticalSection(&appllist_mutex);

		WSACleanup();
		break;
	}
	return TRUE;
}

// The "main" function, the message dispatcher

DWORD initConnection()
{
	DWORD err;

	socke=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(socke==INVALID_SOCKET)
	{
		return 1;
	}

	SOCKADDR_IN server;
	server.sin_family=AF_INET;
	server.sin_port=htons((unsigned short)_port);
	server.sin_addr=toaddr;

	err=connect(socke,(SOCKADDR*)&server,sizeof(server));
	if(err==SOCKET_ERROR)
	{
		closesocket(socke);
		return 2;
	}

	char* msg=(char*)malloc(_MSG_BUFFER);
	if(msg==NULL)
	{
		closesocket(socke);
		return 3;
	}

	REQUEST_HEADER *header=(REQUEST_HEADER*)msg;
	REQUEST_PROXY_HELO *body=(REQUEST_PROXY_HELO*)(msg+sizeof(REQUEST_HEADER));
	char* data=msg+sizeof(REQUEST_HEADER)+sizeof(REQUEST_PROXY_HELO);

	header->header_len=sizeof(REQUEST_HEADER);
	header->body_len=sizeof(REQUEST_PROXY_HELO);
	header->data_len=0;
	header->message_type=TYPE_PROXY_HELO;
	header->message_id=++msg_id;

	memcpy(body->name,(const char*)__myname,64);
	body->os=OS_TYPE_WINDOWS;
	body->version.major=VERSION_MAJOR;
	body->version.minor=VERSION_MINOR;

	header->message_len=sizeof(REQUEST_HEADER)+sizeof(REQUEST_PROXY_HELO);

	err=send(socke,msg,header->message_len,0);
	if(err!=header->message_len)
	{
		closesocket(socke);
		free((void*)msg);
		return 4;
	}

	err=recv(socke,msg,_MSG_BUFFER,0);
	if(err==SOCKET_ERROR || err<sizeof(ANSWER_HEADER))
	{
		closesocket(socke);
		free((void*)msg);
		return 5;
	}


	UINT len=err;
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)msg;
	ANSWER_PROXY_HELO *abody=(ANSWER_PROXY_HELO*)(msg+aheader->header_len);
	char* adata=msg+aheader->header_len+aheader->body_len;

	if(aheader->header_len+aheader->body_len+aheader->data_len!=len ||	aheader->message_len!=len)
	{
		// invalid lengths
		closesocket(socke);
		free((void*)msg);
		return 6;
	}

	if(aheader->proxy_error>0x01)
	{
		// proxy error
		closesocket(socke);
		free((void*)msg);
		return 7;
	}

	if(aheader->message_type!=TYPE_PROXY_HELO)
	{
		// wrong message type
		closesocket(socke);
		free((void*)msg);
		return 8;
	}

	if(aheader->proxy_error==PERROR_AUTH_REQUIRED || AUTH_DESIRED)
	{
		// do_auth(abody->auth_supported);
	}

	max_timeout=abody->timeout;
	// NYI

	session_id=aheader->session_id;
	
	free(msg);

	status=1;
	return 0;
}

DWORD sendRequest(char* msg)
{
	EnterCriticalSection(&socket_mutex);
	DWORD err=send(socke,msg,((REQUEST_HEADER*)msg)->message_len,0);
	free((void*)msg);
	LeaveCriticalSection(&socket_mutex);
	return err;
}

DWORD addToHash(UINT _msgId, char** _data)
{
	evlhash* queue=hash_start;

	EnterCriticalSection(&hash_mutex);
	if(queue)
	{
        while(queue->next)
		{
			queue=queue->next;
		}
		queue->next=(evlhash*)malloc(sizeof(evlhash));
		if(!queue->next)
		{		
			LeaveCriticalSection(&hash_mutex);
			return 1;
		}
		queue=queue->next;	
	} else {
		hash_start=(evlhash*)malloc(sizeof(evlhash));
		if(!hash_start)
		{
			LeaveCriticalSection(&hash_mutex);
			return 1;
		}

		queue=hash_start;
	}

	queue->next=NULL;

	queue->msgId=_msgId;
	queue->data=_data;

	LeaveCriticalSection(&hash_mutex);
	return 0;
}

DWORD removeFromHash(UINT _msgId)
{
	evlhash* queue=hash_start;
	evlhash* prev=NULL;

	EnterCriticalSection(&hash_mutex);
	while(queue)
	{
		if(queue->msgId==_msgId)
		{
			if(prev)
			{
				prev->next=queue->next;
			} else {
				hash_start=queue->next;
			}

			free(queue);
			LeaveCriticalSection(&hash_mutex);
			return 0;
		}
		prev=queue;
		queue=queue->next;
	}
	
	LeaveCriticalSection(&hash_mutex);
	return 0;
}

char** getDataPtrFromMsgId(UINT _msgId)
{
	EnterCriticalSection(&hash_mutex);
	evlhash* queue=hash_start;

	while(queue)
	{
		if(queue->msgId==_msgId)
		{
			char** data=queue->data;
			LeaveCriticalSection(&hash_mutex);
			return data;
		}
		queue=queue->next;
	}

	LeaveCriticalSection(&hash_mutex);
	return NULL;
}

void add_app(DWORD _ApplID, appl_list** _list)
{
	EnterCriticalSection(&appllist_mutex);

	if(*_list!=NULL)
	{
		appl_list* list=(*_list);
		while(list->next)
		{
			list=list->next;
		}

		list->next=(appl_list*)malloc(sizeof(appl_list));
		
		list=list->next;
		list->ApplID=_ApplID;
		list->next=NULL;
		list->data=NULL;
	} else {
		(*_list)=(appl_list*)malloc(sizeof(appl_list));
		(*_list)->ApplID=_ApplID;
		(*_list)->next=NULL;
		(*_list)->data=NULL;
	}

	LeaveCriticalSection(&appllist_mutex);
}

void del_app(DWORD _ApplID, appl_list** _list)
{
	EnterCriticalSection(&appllist_mutex);

	if((*_list)!=NULL)
	{
		appl_list* list=(*_list);
		appl_list* prev=NULL;
		
		while(list!=NULL)
		{
			if(list->ApplID==_ApplID)
			{
				if(prev!=NULL)
				{
					prev->next=list->next;
				} else {
					(*_list)=list->next;
				}
				
				if(list->data!=NULL)
				{
					GlobalFree(list->data);
				}

				free((void*)list);
				
				LeaveCriticalSection(&appllist_mutex);
				return;
			}
			prev=list;
			list=list->next;
		}
	}

	LeaveCriticalSection(&appllist_mutex);
}

DWORD getfirst_app(appl_list** _list)
{
	EnterCriticalSection(&appllist_mutex);

	if((*_list)==NULL)
	{
		LeaveCriticalSection(&appllist_mutex);
		return 0;
	} else {
		appl_list* list=(*_list);
		DWORD ret=list->ApplID;

		(*_list)=list->next;

		if(list->data!=NULL)
		{
			GlobalFree(list->data);
		}
		
		free((void*)list);
		
		LeaveCriticalSection(&appllist_mutex);
		return ret;
	}
	
	LeaveCriticalSection(&appllist_mutex);
}

void* getdata_app(DWORD _ApplID, appl_list* list)
{
	EnterCriticalSection(&appllist_mutex);

	while(list!=NULL)
	{
		if(list->ApplID==_ApplID)
		{
			LeaveCriticalSection(&appllist_mutex);
			return list->data;
		}
		list=list->next;
	}

	LeaveCriticalSection(&appllist_mutex);
	return NULL;
}

void setdata_app(DWORD _ApplID, appl_list* list, void* _data)
{
	EnterCriticalSection(&appllist_mutex);

	while(list!=NULL)
	{
		if(list->ApplID==_ApplID)
		{
			if(list->data!=NULL)
			{
				GlobalFree(list->data);
			}

			list->data=_data;
			
			LeaveCriticalSection(&appllist_mutex);
			return;
		}
		list=list->next;
	}

	//error (entry for _ApplID not found)
	GlobalFree(_data); // so at least we don't produce memory leaks
	LeaveCriticalSection(&appllist_mutex);
}

bool verifyMessage(char* msg, UINT type)
{
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)msg;

	if(aheader->header_len<sizeof(ANSWER_HEADER))
	{
		// header too short
		return false;
	}

	if(aheader->body_len+1<(unsigned int)abodysize(type))
	{
		//  too short body
		return false;
	}

	if(aheader->message_len>_MSG_BUFFER)
	{
		// message too long
		return false;
	}
	
	if(aheader->header_len+aheader->body_len+aheader->data_len!=aheader->message_len)
	{
		// invalid lengths
		return false;
	}

	if(aheader->proxy_error!=0x00)
	{
		// proxy error
		return false;
	}

	if(aheader->capi_error!=0x0000)
	{
		return true;
	}

	if(aheader->message_type!=type)
	{
		// wrong message type
		return false;
	}

	return true;
}

DWORD sendAndReceive(UINT msgId, char* request, char** answer)
{
	DWORD err;

	*answer=NULL;
	addToHash(msgId,answer);
	err=sendRequest(request);

	if((err==0) || (err==SOCKET_ERROR))
	{
		*answer=ILLEGAL_ANSWER;
		return SOCKET_ERROR;
	}
	
	UINT timeout=_WAITFORMESSAGE_TIMEOUT;

	while(*answer==NULL)
	{
		if(timeout) // if not timeout disabled (timeout=0)
		{
		   if(--timeout==0) // if timeout occurs
		   {
			   *answer=ILLEGAL_ANSWER;
			   removeFromHash(msgId);
			   break;
		   }
		}
		Sleep(1);	// Ok, the "beautiful" method didn't work :-( brute force RULZ!
	}


	return (DWORD)*answer;
}

void dispatchMessage(char* msg)
{
	ANSWER_HEADER *header=(ANSWER_HEADER*)msg;
	char** data=getDataPtrFromMsgId(header->message_id);

	removeFromHash(header->message_id);
	*data=msg;
}

DWORD WINAPI messageDispatcher(LPVOID param)
{
	DWORD err;

	while(true)
	{
		void* msg=malloc(_MSG_BUFFER);
		if(msg==NULL)
		{
			break;
		}

		err=recv(socke,(char*)msg,_MSG_BUFFER,0);

		if(err==SOCKET_ERROR || err<sizeof(ANSWER_HEADER))
		{
			free(msg);

			if(err==0 || err==SOCKET_ERROR)
				return SOCKET_ERROR;		// if the socket has been shut down or an error occurs we quit
		} else {
			dispatchMessage((char*)msg);
		}
	}

	return 0;
}



// Here come the exported CAPI 2.0 Functions


DWORD APIENTRY CAPI_REGISTER(DWORD MessageBufferSize,
						   DWORD maxLogicalConnection,
						   DWORD maxBDataBlocks,
						   DWORD maxBDataLen,
						   DWORD *pApplID)
{
	char* request=(char*)malloc(_MSG_BUFFER);
	if(request==NULL)
	{
		return 0x1108;
	}

	REQUEST_HEADER *rheader=(REQUEST_HEADER*)request;
	REQUEST_CAPI_REGISTER *rbody=(REQUEST_CAPI_REGISTER*)(request+sizeof(REQUEST_HEADER));
	char* rdata=request+sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_REGISTER);
	UINT id=++msg_id;

	rheader->header_len=sizeof(REQUEST_HEADER);
	rheader->body_len=sizeof(REQUEST_CAPI_REGISTER);
	rheader->data_len=0;
	rheader->message_type=TYPE_CAPI_REGISTER;
	rheader->message_id=id;

	rheader->session_id=session_id;

	rbody->messageBufferSize=MessageBufferSize;
	rbody->maxLogicalConnection=maxLogicalConnection;
	rbody->maxBDataBlocks=maxBDataBlocks;
	rbody->maxBDataLen=maxBDataLen;

	rheader->message_len=sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_REGISTER);
	// Ok, the message is ready, now let us send it and wait for the answer
	
	char* answer; // The pointer that will point to the answer

	sendAndReceive(id,request,&answer);
	// when the function terminates we have received a message

	if(answer==ILLEGAL_ANSWER)	// or maybe not
	{
		return 0x1108;
	}


	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_REGISTER *abody=(ANSWER_CAPI_REGISTER*)(answer+aheader->header_len);
	char* adata=answer+aheader->header_len+aheader->body_len;

	if(!verifyMessage(answer,TYPE_CAPI_REGISTER))
	{
        free((void*)answer);
		return 0x1108;
	}	
	
	*pApplID=aheader->app_id;

	DWORD capi_error=aheader->capi_error;
	UINT proxy_error=aheader->proxy_error;
	free((void*)answer);

	if(proxy_error!=PERROR_NONE)
	{
		return 0x1108;
	}

	if(capi_error==0x0000)
	{
		add_app(*pApplID,&registered_apps);
	}

	return capi_error;
}

DWORD APIENTRY CAPI_RELEASE(DWORD ApplID)
{
	char* request=(char*)malloc(_MSG_BUFFER);
	if(request==NULL)
	{
		return 0x1108;
	}

	REQUEST_HEADER *rheader=(REQUEST_HEADER*)request;
	REQUEST_CAPI_RELEASE *rbody=(REQUEST_CAPI_RELEASE*)(request+sizeof(REQUEST_HEADER));
	char* rdata=request+sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_RELEASE);
	UINT id=++msg_id;

	rheader->header_len=sizeof(REQUEST_HEADER);
	rheader->body_len=sizeof(REQUEST_CAPI_RELEASE);
	rheader->data_len=0;
	rheader->message_type=TYPE_CAPI_RELEASE;
	rheader->message_id=id;

	rheader->app_id=ApplID;
	rheader->session_id=session_id;

	rheader->message_len=sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_RELEASE);
	// Ok, the message is ready, now let us send it and wait for the answer
	
	char* answer; // The pointer that will point to the answer

	sendAndReceive(id,request,&answer);
	// when the function terminates we have received a message

	if(answer==ILLEGAL_ANSWER)	// or maybe not
	{
		return 0x1108;
	}


	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_RELEASE *abody=(ANSWER_CAPI_RELEASE*)(answer+aheader->header_len);
	char* adata=answer+aheader->header_len+aheader->body_len;

	if(!verifyMessage(answer,TYPE_CAPI_RELEASE))
	{
        free((void*)answer);
		return 0x1108;
	}	

	DWORD capi_error=aheader->capi_error;
	UINT proxy_error=aheader->proxy_error;
	free((void*)answer);

	if(proxy_error!=PERROR_NONE)
	{
		return 0x1108;
	}

	if(capi_error==0x0000)
	{
		del_app(ApplID,&registered_apps);
	}

	return capi_error;
}

DWORD APIENTRY CAPI_PUT_MESSAGE(DWORD ApplID,
							  LPVOID pCAPIMessage)
{
	char *data1,*data2;
	UINT len1=0,len2=0;

	data1=(char*)pCAPIMessage;

	char* request=(char*)malloc(_MSG_BUFFER);
	if(request==NULL)
	{
		return 0x1108;
	}

	REQUEST_HEADER *rheader=(REQUEST_HEADER*)request;
	REQUEST_CAPI_PUTMESSAGE *rbody=(REQUEST_CAPI_PUTMESSAGE*)(request+sizeof(REQUEST_HEADER));
	char* rdata=request+sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_PUTMESSAGE);
	UINT id=++msg_id;

	rheader->header_len=sizeof(REQUEST_HEADER);
	rheader->body_len=sizeof(REQUEST_CAPI_PUTMESSAGE);
	rheader->data_len=0;
	rheader->message_type=TYPE_CAPI_PUTMESSAGE;
	rheader->message_id=id;

	rheader->app_id=ApplID;
	rheader->session_id=session_id;
	//rheader->controller_id=msgGetController(data1); // The server will parse the message anyways
	
	memcpy(&len1,data1,2);
	memcpy(rdata,data1,len1);	// copy the CAPI message to the message
	rheader->data_len+=len1;	// add the CAPI message length to the data_length

	unsigned char cmd1=CAPI_CMD1(data1);
	unsigned char cmd2=CAPI_CMD2(data1);

	if((cmd1==0x86) && (cmd2==0x80)) // If we got a DATA_B3 REQUEST
	{
		memcpy(&len2,data1+16,2);		// Get the data length from the CAPI message
		DWORD pointer;
		memcpy(&pointer,data1+12,4);
		data2=(char*)(LPVOID)pointer;	// This is the pointer to the data

		memcpy(rdata+len1,data2,len2);	// add the data to the message
		rheader->data_len+=len2;
	}

	rheader->message_len=sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_PUTMESSAGE)+len1+len2;
	// Ok, the message is ready, now let us send it and wait for the answer
	
	char* answer; // The pointer that will point to the answer

	sendAndReceive(id,request,&answer);
	// when the function terminates we have received a message

	if(answer==ILLEGAL_ANSWER)	// or maybe not
	{
		return 0x1108;
	}


	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_PUTMESSAGE *abody=(ANSWER_CAPI_PUTMESSAGE*)(answer+aheader->header_len);
	char* adata=answer+aheader->header_len+aheader->body_len;

	if(!verifyMessage(answer,TYPE_CAPI_PUTMESSAGE))
	{
        free((void*)answer);
		return 0x1108;
	}	

	DWORD capi_error=aheader->capi_error;
	UINT proxy_error=aheader->proxy_error;
	free((void*)answer);

	if(proxy_error!=PERROR_NONE)
	{
		return 0x1108;
	}

	return capi_error;
}

DWORD APIENTRY CAPI_GET_MESSAGE(DWORD ApplID, LPVOID *ppCAPIMessage)
{
	UINT len1=0,len2=0;

	char* request=(char*)malloc(_MSG_BUFFER);
	if(request==NULL)
	{
		return 0x1108;
	}

	REQUEST_HEADER *rheader=(REQUEST_HEADER*)request;
	REQUEST_CAPI_GETMESSAGE *rbody=(REQUEST_CAPI_GETMESSAGE*)(request+sizeof(REQUEST_HEADER));
	char* rdata=request+sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_GETMESSAGE);
	UINT id=++msg_id;

	rheader->header_len=sizeof(REQUEST_HEADER);
	rheader->body_len=sizeof(REQUEST_CAPI_GETMESSAGE);
	rheader->data_len=0;
	rheader->message_type=TYPE_CAPI_GETMESSAGE;
	rheader->message_id=id;

	rheader->app_id=ApplID;
	rheader->session_id=session_id;

	rheader->message_len=sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_GETMESSAGE);
	// Ok, the message is ready, now let us send it and wait for the answer
	
	char* answer; // The pointer that will point to the answer

	sendAndReceive(id,request,&answer);
	// when the function terminates we have received a message

	if(answer==ILLEGAL_ANSWER)	// or maybe not
	{
		return 0x1108;
	}


	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_GETMESSAGE *abody=(ANSWER_CAPI_GETMESSAGE*)(answer+aheader->header_len);
	char* adata=answer+aheader->header_len+aheader->body_len;

	if(!verifyMessage(answer,TYPE_CAPI_GETMESSAGE))
	{
        free((void*)answer);
		return 0x1108;
	}	
	
	if(aheader->data_len<8) // data too short
	{
		free((void*)answer);
		return 0x1108;
	}

	void* buffer=GlobalAlloc(GPTR,aheader->data_len);
	if(buffer==NULL)
	{
		return 0x1108;
	}
	memcpy(buffer,adata,aheader->data_len);

	memcpy(&len1,buffer,2);		// get the length of the CAPI message
	if(aheader->data_len<len1) // data too short
	{
		free((void*)answer);
		return 0x1108;
	}

	unsigned char cmd1=CAPI_CMD1(buffer);
	unsigned char cmd2=CAPI_CMD2(buffer);

	if((cmd1==0x86) && (cmd2==0x82))	// If we got a DATA_B3 IND
	{
		if(len1<18){
			free((void*)answer);
			return 0x1108;
		}
		
		memcpy(&len2,((char*)buffer)+16,2);	// get the lengtzh of the data

		DWORD pointer=(DWORD)buffer+len1;		// data is appended to the message
		memcpy(((char*)buffer)+12,&pointer,4);
	}

	*ppCAPIMessage=buffer;

	setdata_app(ApplID,registered_apps,buffer);

	DWORD capi_error=aheader->capi_error;
	UINT proxy_error=aheader->proxy_error;
	free((void*)answer);

	if(proxy_error!=PERROR_NONE)
	{
		return 0x1108;
	}

	return capi_error;
}

DWORD APIENTRY CAPI_WAIT_FOR_SIGNAL(DWORD ApplID)
{
	char* request=(char*)malloc(_MSG_BUFFER);
	if(request==NULL)
	{
		return 0x1108;
	}

	REQUEST_HEADER *rheader=(REQUEST_HEADER*)request;
	REQUEST_CAPI_WAITFORSIGNAL *rbody=(REQUEST_CAPI_WAITFORSIGNAL*)(request+sizeof(REQUEST_HEADER));
	char* rdata=request+sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_WAITFORSIGNAL);
	UINT id=++msg_id;

	rheader->header_len=sizeof(REQUEST_HEADER);
	rheader->body_len=sizeof(REQUEST_CAPI_WAITFORSIGNAL);
	rheader->data_len=0;
	rheader->message_type=TYPE_CAPI_WAITFORSIGNAL;
	rheader->message_id=id;

	rheader->app_id=ApplID;
	rheader->session_id=session_id;

	rheader->message_len=sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_WAITFORSIGNAL);
	// Ok, the message is ready, now let us send it and wait for the answer
	
	char* answer; // The pointer that will point to the answer

	sendAndReceive(id,request,&answer);
	// when the function terminates we have received a message

	if(answer==ILLEGAL_ANSWER)	// or maybe not
	{
		return 0x1108;
	}


	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_WAITFORSIGNAL *abody=(ANSWER_CAPI_WAITFORSIGNAL*)(answer+aheader->header_len);
	char* adata=answer+aheader->header_len+aheader->body_len;

	if(!verifyMessage(answer,TYPE_CAPI_WAITFORSIGNAL))
	{
        free((void*)answer);
		return 0x1108;
	}	

	DWORD capi_error=aheader->capi_error;
	UINT proxy_error=aheader->proxy_error;
	free((void*)answer);

	if(proxy_error!=PERROR_NONE)
	{
		return 0x1108;
	}

	return capi_error;
}

void APIENTRY CAPI_GET_MANUFACTURER(char* szBuffer)
{
	char* request=(char*)malloc(_MSG_BUFFER);
	if(request==NULL)
	{
		return;
	}

	REQUEST_HEADER *rheader=(REQUEST_HEADER*)request;
	REQUEST_CAPI_MANUFACTURER *rbody=(REQUEST_CAPI_MANUFACTURER*)(request+sizeof(REQUEST_HEADER));
	char* rdata=request+sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_MANUFACTURER);
	UINT id=++msg_id;

	rheader->header_len=sizeof(REQUEST_HEADER);
	rheader->body_len=sizeof(REQUEST_CAPI_MANUFACTURER);
	rheader->data_len=0;
	rheader->message_type=TYPE_CAPI_MANUFACTURER;
	rheader->message_id=id;

	rheader->app_id=0;
	rheader->session_id=session_id;

	rheader->message_len=sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_MANUFACTURER);
	// Ok, the message is ready, now let us send it and wait for the answer
	
	char* answer; // The pointer that will point to the answer

	sendAndReceive(id,request,&answer);
	// when the function terminates we have received a message

	if(answer==ILLEGAL_ANSWER)	// or maybe not
	{
		return;
	}


	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_MANUFACTURER *abody=(ANSWER_CAPI_MANUFACTURER*)(answer+aheader->header_len);
	char* adata=answer+aheader->header_len+aheader->body_len;

	if(!verifyMessage(answer,TYPE_CAPI_MANUFACTURER))
	{
        free((void*)answer);
		return;
	}	

	memcpy(szBuffer,abody->manufacturer,64);	// copy data to the result

	DWORD capi_error=aheader->capi_error;
	UINT proxy_error=aheader->proxy_error;
	free((void*)answer);

	if(proxy_error!=PERROR_NONE)
	{
		return;
	}

	return;
}

DWORD APIENTRY CAPI_GET_VERSION(DWORD * pCAPIMajor,
							  DWORD * pCAPIMinor,
							  DWORD * pManufacturerMajor,
							  DWORD * pManufacturerMinor)
{
	char* request=(char*)malloc(_MSG_BUFFER);
	if(request==NULL)
	{
		return 0x1108;
	}

	REQUEST_HEADER *rheader=(REQUEST_HEADER*)request;
	REQUEST_CAPI_VERSION *rbody=(REQUEST_CAPI_VERSION*)(request+sizeof(REQUEST_HEADER));
	char* rdata=request+sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_VERSION);
	UINT id=++msg_id;

	rheader->header_len=sizeof(REQUEST_HEADER);
	rheader->body_len=sizeof(REQUEST_CAPI_VERSION);
	rheader->data_len=0;
	rheader->message_type=TYPE_CAPI_VERSION;
	rheader->message_id=id;

	rheader->app_id=0;
	rheader->session_id=session_id;

	rheader->message_len=sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_VERSION);
	// Ok, the message is ready, now let us send it and wait for the answer
	
	char* answer; // The pointer that will point to the answer

	sendAndReceive(id,request,&answer);
	// when the function terminates we have received a message

	if(answer==ILLEGAL_ANSWER)	// or maybe not
	{
		return 0x1108;
	}


	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_VERSION *abody=(ANSWER_CAPI_VERSION*)(answer+aheader->header_len);
	char* adata=answer+aheader->header_len+aheader->body_len;

	if(!verifyMessage(answer,TYPE_CAPI_VERSION))
	{
        free((void*)answer);
		return 0x1108;
	}
	
	*pCAPIMajor=abody->driver.major;
	*pCAPIMinor=abody->driver.minor;
	*pManufacturerMajor=abody->manufacturer.major;
	*pManufacturerMinor=abody->manufacturer.minor;

	DWORD capi_error=aheader->capi_error;
	UINT proxy_error=aheader->proxy_error;
	free((void*)answer);

	if(proxy_error!=PERROR_NONE)
	{
		return 0x1108;
	}

	return capi_error;
}

DWORD APIENTRY CAPI_GET_SERIAL_NUMBER(char* szBuffer)
{
	char* request=(char*)malloc(_MSG_BUFFER);
	if(request==NULL)
	{
		return 0x1108;
	}

	REQUEST_HEADER *rheader=(REQUEST_HEADER*)request;
	REQUEST_CAPI_SERIAL *rbody=(REQUEST_CAPI_SERIAL*)(request+sizeof(REQUEST_HEADER));
	char* rdata=request+sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_SERIAL);
	UINT id=++msg_id;

	rheader->header_len=sizeof(REQUEST_HEADER);
	rheader->body_len=sizeof(REQUEST_CAPI_SERIAL);
	rheader->data_len=0;
	rheader->message_type=TYPE_CAPI_SERIAL;
	rheader->message_id=id;

	rheader->app_id=0;
	rheader->session_id=session_id;

	rheader->message_len=sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_SERIAL);
	// Ok, the message is ready, now let us send it and wait for the answer
	
	char* answer; // The pointer that will point to the answer

	sendAndReceive(id,request,&answer);
	// when the function terminates we have received a message

	if(answer==ILLEGAL_ANSWER)	// or maybe not
	{
		return 0x1108;
	}


	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_SERIAL *abody=(ANSWER_CAPI_SERIAL*)(answer+aheader->header_len);
	char* adata=answer+aheader->header_len+aheader->body_len;

	if(!verifyMessage(answer,TYPE_CAPI_SERIAL))
	{
        free((void*)answer);
		return 0x1108;
	}	

	memcpy(szBuffer,abody->serial,8);		// Copy the serial to the result

	DWORD capi_error=aheader->capi_error;
	UINT proxy_error=aheader->proxy_error;
	free((void*)answer);

	if(proxy_error!=PERROR_NONE)
	{
		return 0x1108;
	}

	return capi_error;
}

DWORD APIENTRY CAPI_GET_PROFILE(LPVOID szBuffer,
								DWORD CtrlNr)
{
	char* request=(char*)malloc(_MSG_BUFFER);
	if(request==NULL)
	{
		return 0x1108;
	}

	REQUEST_HEADER *rheader=(REQUEST_HEADER*)request;
	REQUEST_CAPI_PROFILE *rbody=(REQUEST_CAPI_PROFILE*)(request+sizeof(REQUEST_HEADER));
	char* rdata=request+sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_PROFILE);
	UINT id=++msg_id;

	rheader->header_len=sizeof(REQUEST_HEADER);
	rheader->body_len=sizeof(REQUEST_CAPI_PROFILE);
	rheader->data_len=0;
	rheader->message_type=TYPE_CAPI_PROFILE;
	rheader->message_id=id;

	rheader->app_id=0;
	rheader->controller_id=CtrlNr;
	rheader->session_id=session_id;

	rheader->message_len=sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_PROFILE);
	// Ok, the message is ready, now let us send it and wait for the answer
	
	char* answer; // The pointer that will point to the answer

	sendAndReceive(id,request,&answer);
	// when the function terminates we have received a message

	if(answer==ILLEGAL_ANSWER)	// or maybe not
	{
		return 0x1108;
	}


	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_PROFILE *abody=(ANSWER_CAPI_PROFILE*)(answer+aheader->header_len);
	char* adata=answer+aheader->header_len+aheader->body_len;

	if(!verifyMessage(answer,TYPE_CAPI_PROFILE))
	{
        free((void*)answer);
		return 0x1108;
	}	

	memcpy(szBuffer,abody->profile,64);		// copy the profile to the result
	
	DWORD capi_error=aheader->capi_error;
	UINT proxy_error=aheader->proxy_error;
	free((void*)answer);

	if(proxy_error!=PERROR_NONE)
	{
		return 0x1108;
	}

	return capi_error;
}

DWORD APIENTRY CAPI_INSTALLED(void)
{
	char* request=(char*)malloc(_MSG_BUFFER);
	if(request==NULL)
	{
		return 0x1108;
	}

	REQUEST_HEADER *rheader=(REQUEST_HEADER*)request;
	REQUEST_CAPI_INSTALLED *rbody=(REQUEST_CAPI_INSTALLED*)(request+sizeof(REQUEST_HEADER));
	char* rdata=request+sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_INSTALLED);
	UINT id=++msg_id;

	rheader->header_len=sizeof(REQUEST_HEADER);
	rheader->body_len=sizeof(REQUEST_CAPI_INSTALLED);
	rheader->data_len=0;
	rheader->message_type=TYPE_CAPI_INSTALLED;
	rheader->message_id=id;

	rheader->app_id=0;
	rheader->session_id=session_id;

	rheader->message_len=sizeof(REQUEST_HEADER)+sizeof(REQUEST_CAPI_INSTALLED);
	// Ok, the message is ready, now let us send it and wait for the answer
	
	char* answer; // The pointer that will point to the answer

	sendAndReceive(id,request,&answer);
	// when the function terminates we have received a message

	if(answer==ILLEGAL_ANSWER)	// or maybe not
	{
		return 0x1108;
	}

	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_INSTALLED *abody=(ANSWER_CAPI_INSTALLED*)(answer+aheader->header_len);
	char* adata=answer+aheader->header_len+aheader->body_len;

	if(!verifyMessage(answer,TYPE_CAPI_INSTALLED))
	{
        free((void*)answer);
		return 0x1108;
	}	

	DWORD capi_error=aheader->capi_error;
	UINT proxy_error=aheader->proxy_error;
	free((void*)answer);

	if(proxy_error!=PERROR_NONE)
	{
		return 0x1108;
	}

	return capi_error;
}
