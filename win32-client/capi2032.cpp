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

#define VERSION_MAJOR	0
#define VERSION_MINOR	1


extern "C"
{
	c_register		old_register;
	c_release		old_release;
	c_put_message	old_put_message;
	c_get_message	old_get_message;
	c_wait_for_signal	old_wait_for_signal;
	c_get_manufacturer	old_get_manufacturer;
	c_get_version	old_get_version;
	c_get_serial	old_get_serial;
	c_get_profile	old_get_profile;
	c_installed		old_installed;
}

unsigned char ctrl_map[MAX_CONTROLLERS+1];

LPVOID GlobalBuffer=NULL;

IN_ADDR toaddr;

HMODULE dll;	
UINT local_controllers, remote_controllers, controllers;
applmap application_map[MAX_APPLICATIONS];
int status=0;
DWORD msg_id=0;
char* __myname[64];
SOCKET socke;
int max_timeout=-1, timeout=0;
HANDLE receiver;
int run=1, pause=0;
evlhash* hash_start=NULL;
UINT session_id=0;

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
			::MessageBox(NULL,"Could not open Registry Key \"HKEY_LOCAL_MACHINE\\Software\\The Reg Guy\\capi20proxy\"","Error",MB_OK);
			return FALSE;
		}
		
		len=256;
		err=RegQueryValueEx(key,"server",NULL,NULL,(unsigned char*)ipstr,&len);
		if(err!=ERROR_SUCCESS){
			::MessageBox(NULL,"Could not read Regitry Value \"HKEY_LOCAL_MACHINE\\Software\\The Reg Guy\\capi20proxy\\server\"","Error",MB_OK);
			return FALSE;
		}
		toaddr.S_un.S_addr=inet_addr(ipstr);

		len=64;
		err=RegQueryValueEx(key,"name",NULL,NULL,(unsigned char*)__myname,&len);
		if(err!=ERROR_SUCCESS){
			::MessageBox(NULL,"Could not read Regitry Value \"HKEY_LOCAL_MACHINE\\Software\\The Reg Guy\\capi20proxy\\name\"","Error",MB_OK);
			return FALSE;
		}
		toaddr.S_un.S_addr=inet_addr(ipstr);

		RegCloseKey(key);

		dll = LoadLibraryEx("oldcapi2032.dll",NULL,0);

		if (dll==NULL)
			return FALSE;

		old_register=(c_register)GetProcAddress(dll,(LPCTSTR)1);
		old_release=(c_release)GetProcAddress(dll,(LPCTSTR)2);
		old_put_message=(c_put_message)GetProcAddress(dll,(LPCTSTR)3);
		old_get_message=(c_get_message)GetProcAddress(dll,(LPCTSTR)4);
		old_wait_for_signal=(c_wait_for_signal)GetProcAddress(dll,(LPCTSTR)5);
		old_get_manufacturer=(c_get_manufacturer)GetProcAddress(dll,(LPCTSTR)6);
		old_get_version=(c_get_version)GetProcAddress(dll,(LPCTSTR)7);
		old_get_serial=(c_get_serial)GetProcAddress(dll,(LPCTSTR)8);
		old_get_profile=(c_get_profile)GetProcAddress(dll,(LPCTSTR)9);
		old_installed=(c_installed)GetProcAddress(dll,(LPCTSTR)10);

		initConnection();

		initControllerNum();

		DWORD thid;
		receiver=CreateThread(NULL,1024,messageDispatcher,NULL,0,&thid);
		break;
 
	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:

		if(GlobalBuffer!=NULL) free(GlobalBuffer);

		run=0;
		if(WaitForSingleObject(receiver,5000)!=WAIT_OBJECT_0)
		{
			shutdownConnection();
			TerminateThread(receiver,1);
		} else {
			shutdownConnection();
		}

		WSACleanup();

		FreeLibrary(dll);
		break;
	}
	return TRUE;
}

UINT getLocalApp(UINT _remote)
{
	for(int i=0; i<MAX_APPLICATIONS; i++)
	{
		if(application_map[i].remote==_remote)
			return application_map[i].local;
	}

	return 0;
}

UINT getRemoteApp(UINT _local)
{
	for(int i=0; i<MAX_APPLICATIONS; i++)
	{
		if(application_map[i].local==_local)
			return application_map[i].remote;
	}

	return 0;
}


void delApp(UINT _local)
{
	for(int i=0; i<MAX_APPLICATIONS; i++)
	{
		if(application_map[i].local==_local)
		{
			application_map[i].remote=0;
			application_map[i].local=0;
		}
	}
}

bool addApp(UINT _local, UINT _remote)
{
	for(int i=0; i<MAX_APPLICATIONS; i++)
	{
		if(application_map[i].local==0)
		{
			application_map[i].remote=_remote;
			application_map[i].local=_local;
			return true;
		}
	}

	return false;
}

UINT msgGetApplId(char* _msg)
{
	return (*((UINT*)(_msg+2)));
}

void msgSetApplId(char* _msg, UINT _applid)
{
	 *((UINT*)(_msg+2))=_applid;
}

unsigned char msgGetController(char* _msg)
{
	return ((unsigned char) ((*((DWORD*)(_msg+8))) & 0x7F));
}

void msgSetController(char* _msg, unsigned char _ctrl)
{
	DWORD& ncci=(*((DWORD*)(_msg+8)));
	ncci &=  0xFFFFFF80;
	ncci += _ctrl;
}

void initControllerNum()
{
	// falsch, da CAPI_GET_CONTROLLER schon die Gesamtzahl zurückgibt
	DWORD err;
	char buffer[64];
	
	err=old_get_profile(buffer, 0);		//get the local CAPI profile
	if(err==0x0000)
	{
		local_controllers=*((int*)buffer);
	} else {
		local_controllers=0;
	}

	err=CAPI_GET_PROFILE(buffer, 0);		//get the remote CAPI profile
	if(err==0x0000)
	{
		remote_controllers=*((int*)buffer);
	} else {
		remote_controllers=0;
	}

	if(local_controllers>MAX_CONTROLLERS) local_controllers=MAX_CONTROLLERS;
	if(remote_controllers>MAX_CONTROLLERS) remote_controllers=MAX_CONTROLLERS;

	controllers=local_controllers+remote_controllers;

	if(controllers>MAX_CONTROLLERS) controllers=MAX_CONTROLLERS;
}

int SocketSend(SOCKET s, const char *buf, int len, int flags, DWORD timeout){
	fd_set sockets;
	timeval select_timeout;
	int ret;
	
	select_timeout.tv_sec=timeout;
	select_timeout.tv_usec=0;

	FD_ZERO(&sockets);
	FD_SET(s,&sockets);
	ret=select(0,NULL,&sockets,NULL,&select_timeout);

	if((ret==0) || (ret==SOCKET_ERROR)){
		return SOCKET_ERROR;
	} else {
		return send(s,buf,len,flags);
	}
}

int SocketReceive(SOCKET s, char *buf, int len, int flags, DWORD timeout){
	fd_set sockets;
	timeval select_timeout;
	int ret;
	
	select_timeout.tv_sec=timeout;
	select_timeout.tv_usec=0;

	FD_ZERO(&sockets);
	FD_SET(s,&sockets);
	ret=select(0,&sockets,NULL,NULL,&select_timeout);

	if((ret==0) || (ret==SOCKET_ERROR)){
		return SOCKET_ERROR;
	} else {
		return recv(s,buf,len,flags);
	}
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
	server.sin_port=htons(__PORT);
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

	err=SocketSend(socke,msg,header->message_len,0,5);
	if(err!=header->message_len)
	{
		closesocket(socke);
		free((void*)msg);
		return 4;
	}

	err=SocketReceive(socke,msg,_MSG_BUFFER,0,10);
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

DWORD shutdownConnection()
{
	return closesocket(socke);
}

DWORD sendRequest(char* msg)
{
	return SocketSend(socke,msg,((REQUEST_HEADER*)msg)->message_len,0,5);
}

DWORD addToHash(UINT _msgId, char** _data, HANDLE _thread)
{
	evlhash* queue=hash_start;

	if(queue)
	{
        while(queue->next)
		{
			queue=queue->next;
		}
		queue->next=(evlhash*)malloc(sizeof(evlhash));
		if(!queue->next)
		{		
			return 1;
		}
		queue=queue->next;	
	} else {
		hash_start=(evlhash*)malloc(sizeof(evlhash));
		if(!hash_start)
		{
			return 1;
		}

		queue=hash_start;
	}

	queue->next=NULL;

	queue->msgId=_msgId;
	queue->thread=_thread;
	queue->data=_data;

	return 0;
}

DWORD removeFromHash(UINT _msgId)
{
	evlhash* queue=hash_start;
	evlhash* prev=NULL;

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
		}
		prev=queue;
		queue=queue->next;
	}
	
	return 0;
}

HANDLE getThreadFromMsgId(UINT _msgId)
{
	evlhash* queue=hash_start;

	while(queue)
	{
		if(queue->msgId==_msgId)
			return queue->thread;
	}

	return NULL;
}

char** getDataPtrFromMsgId(UINT _msgId)
{
	evlhash* queue=hash_start;

	while(queue)
	{
		if(queue->msgId==_msgId)
			return queue->data;
	}

	return NULL;
}

bool verifyMessage(char* msg, UINT type)
{
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)msg;

	if(aheader->header_len<sizeof(ANSWER_HEADER))
	{
		// header too short
		return false;
	}

	if(aheader->body_len<(unsigned int)abodysize(type))
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

	if(aheader->message_type!=type)
	{
		// wrong message type
		return false;
	}

	return true;
}

DWORD waitForAnswer(UINT msgId, char** data)
{
	addToHash(msgId,data,GetCurrentThread());
	return SuspendThread(GetCurrentThread());
}

void dispatchMessage(char* msg)
{
	ANSWER_HEADER *header=(ANSWER_HEADER*)msg;
	HANDLE thread=getThreadFromMsgId(header->message_id);
	char** data=getDataPtrFromMsgId(header->message_id);
	
	*data=msg;
	removeFromHash(header->message_id);
	ResumeThread(thread);
}

DWORD WINAPI messageDispatcher(LPVOID param)
{
	DWORD err;

	while(run==1)
	{
		Sleep(100);
		if(pause!=1)
		{
			fd_set test;
			test.fd_count=1;
			test.fd_array[0]=socke;
			
			timeval time;
			time.tv_sec=0;
			time.tv_usec=500;

			if(test.fd_count!=0)
			{
				void* msg=malloc(_MSG_BUFFER);
				err=SocketReceive(socke,(char*)msg,_MSG_BUFFER,0,10);
				if(err==SOCKET_ERROR || err<sizeof(ANSWER_HEADER))
				{
					free(msg);
				}
				dispatchMessage((char*)msg);				
			}
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
	DWORD local_app=0;
	DWORD err;
	
	// register the application first on the local oldcapi2032.dll
	if(old_register!=NULL)
	{
		err=old_register(MessageBufferSize, maxLogicalConnection, maxBDataBlocks, maxBDataLen, &local_app);
		if(err!=0x0000)
			return err;		// exit if something doesn't work
	}


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
	
	sendRequest(request);
	free((void*)request);

	char* answer; // The pointer that will point to the answer
	waitForAnswer(id,&answer);
	// when the function terminates we are sure to have received a message


	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_REGISTER *abody=(ANSWER_CAPI_REGISTER*)(answer+aheader->header_len);
	char* adata=answer+aheader->header_len+aheader->body_len;

	if(!verifyMessage(answer,TYPE_CAPI_REGISTER))
	{
        free((void*)answer);
		return 0x1108;
	}	
	
	*pApplID=aheader->app_id;

	// the server must exist and work... for we assign local app_ids to remote ones...
	if(local_app!=0)
	{
		if(!addApp((UINT)local_app,(UINT)*pApplID))		// Add the two applIds to the map
		{
			free((void*)answer);
			CAPI_RELEASE(*pApplID);		// release the appId
		}
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

DWORD APIENTRY CAPI_RELEASE(DWORD ApplID)
{
	UINT local_app = getLocalApp(ApplID);
	
	if(old_release!=NULL){
		old_release(local_app);	// release the local appId
	}

	delApp(local_app);		// delete the appId from the map

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
	
	sendRequest(request);
	free((void*)request);

	char* answer; // The pointer that will point to the answer
	waitForAnswer(id,&answer);
	// when the function terminates we are sure to have received a message


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

	return capi_error;
}

DWORD APIENTRY CAPI_PUT_MESSAGE(DWORD ApplID,
							  LPVOID pCAPIMessage)
{
	if(msgGetController((char*)pCAPIMessage)>remote_controllers)
	{
		msgSetController((char*)pCAPIMessage, msgGetController((char*)pCAPIMessage)-remote_controllers);
		msgSetApplId((char*)pCAPIMessage, getLocalApp(ApplID));

		if(old_put_message!=NULL)
		{
			return old_put_message(getLocalApp(ApplID), pCAPIMessage);
		} else {
			return 0x1108;
		}
	}

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
	rheader->controller_id=msgGetController(data1);
	
	memcpy(&len1,data1,2);
	memcpy(rdata,data1,len1);	// copy the CAPI message to the message
	rheader->data_len+=len1;	// add the CAPI message length to the data_length

	unsigned char cmd1=data1[4];
	unsigned char cmd2=data1[5];

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
	
	sendRequest(request);
	free((void*)request);

	char* answer; // The pointer that will point to the answer
	waitForAnswer(id,&answer);
	// when the function terminates we are sure to have received a message


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
	if(old_get_message!=NULL)
	{
		 if(old_get_message(getLocalApp(ApplID), ppCAPIMessage)==0x0000)
		 {
			msgSetController((char*)*ppCAPIMessage, msgGetController((char*)*ppCAPIMessage)+remote_controllers);
			msgSetApplId((char*)*ppCAPIMessage, getRemoteApp(ApplID));
			
			return 0x0000;
		 }
	}

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
	
	sendRequest(request);
	free((void*)request);

	char* answer; // The pointer that will point to the answer
	waitForAnswer(id,&answer);
	// when the function terminates we are sure to have received a message


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

	if(GlobalBuffer!=NULL) GlobalFree(GlobalBuffer);
	// This App is written to deal with only one App....
	// For multiple Applications we must implement a GlobalBuffer for each Application
	GlobalBuffer=GlobalAlloc(GPTR,aheader->data_len);
	if(GlobalBuffer==NULL)
	{
		return 0x1108;
	}
	memcpy(GlobalBuffer,adata,aheader->data_len);

	memcpy(&len1,GlobalBuffer,2);		// get the length of the CAPI message
	if(aheader->data_len<len1) // data too short
	{
		free((void*)answer);
		return 0x1108;
	}

	unsigned char cmd1=((char*)GlobalBuffer)[4];
	unsigned char cmd2=((char*)GlobalBuffer)[5];

	if((cmd1==0x86) && (cmd2==0x82))	// If we got a DATA_B3 IND
	{
		if(len1<18){
			free((void*)answer);
			return 0x1108;
		}
		
		memcpy(&len2,((char*)GlobalBuffer)+16,2);	// get the lengtzh of the data

		DWORD pointer=(DWORD)GlobalBuffer+len1;		// data is appended to the message
		memcpy(((char*)GlobalBuffer)+12,&pointer,4);
	}

	*ppCAPIMessage=GlobalBuffer;

	DWORD capi_error=aheader->capi_error;
	UINT proxy_error=aheader->proxy_error;
	free((void*)answer);

	if(proxy_error!=PERROR_NONE)
	{
		return 0x1108;
	}

	return capi_error;
}

DWORD WINAPI waiting_proc_1(void* param)
{
	DWORD ApplID=(DWORD)param;

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
	
	sendRequest(request);
	free((void*)request);

	char* answer; // The pointer that will point to the answer
	waitForAnswer(id,&answer);
	// when the function terminates we are sure to have received a message


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

DWORD WINAPI waiting_proc_2(void* param)
{
	DWORD ApplID=(DWORD)param;

	if(old_wait_for_signal!=NULL)
	{
		return old_wait_for_signal(ApplID);
	}

	return 0x1108;
}

DWORD APIENTRY CAPI_WAIT_FOR_SIGNAL(DWORD ApplID)
{
	DWORD thid1, thid2;
	HANDLE thread_handles[2] = { NULL, NULL };
	DWORD num=1;

	thread_handles[0]=CreateThread(NULL,1024,waiting_proc_1,(LPVOID)ApplID,0,&thid1);
	if(old_wait_for_signal!=NULL)
	{
		thread_handles[1]=CreateThread(NULL,1024,waiting_proc_2,(LPVOID)ApplID,0,&thid2);
		num=2;
	}

	num=WaitForMultipleObjects(num,thread_handles,FALSE,180*1000);

	if((WAIT_OBJECT_0<=num) && (num<(WAIT_OBJECT_0+2)))
	{
		DWORD error;
		if(GetExitCodeThread(thread_handles[num-WAIT_OBJECT_0],&error))
		{
			return error;
		}
	}

	return 0x1108;	
}

void APIENTRY CAPI_GET_MANUFACTURER(char* szBuffer)
{
	if(old_get_manufacturer!=NULL)
	{
		old_get_manufacturer(szBuffer);
		return;
	}

	szBuffer[0]=0;

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
	
	sendRequest(request);
	free((void*)request);

	char* answer; // The pointer that will point to the answer
	waitForAnswer(id,&answer);
	// when the function terminates we are sure to have received a message


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
	if(old_get_version!=NULL)
	{
		return old_get_version(pCAPIMajor, pCAPIMinor, pManufacturerMajor, pManufacturerMinor);
	}

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
	
	sendRequest(request);
	free((void*)request);

	char* answer; // The pointer that will point to the answer
	waitForAnswer(id,&answer);
	// when the function terminates we are sure to have received a message


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
	if(old_get_serial!=NULL)
	{
		return old_get_serial(szBuffer);
	}

	szBuffer[0]=0;

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
	
	sendRequest(request);
	free((void*)request);

	char* answer; // The pointer that will point to the answer
	waitForAnswer(id,&answer);
	// when the function terminates we are sure to have received a message


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
	DWORD err;
	((char*)szBuffer)[0]=0; //I don't think we need this

	if(CtrlNr>remote_controllers)		// request for a local controller
	{
		if(old_get_profile!=NULL)
		{
			err = old_get_profile(szBuffer, CtrlNr-remote_controllers);
			*((UINT*)szBuffer)=controllers;		// Set the number of returned controllers to the real number of controller we got

			return err;
		}
	}

	if(CtrlNr==0)		// general request to the driver (no specific controller)
	{
		if(old_get_profile!=NULL)
		{
			err = old_get_profile(szBuffer, CtrlNr-remote_controllers);
			*((UINT*)szBuffer)=controllers;		// Set the number of returned controllers to the real number of controller we got

			if(err==0x0000)		// if the local .dll got problems try to pass the request to the remote server
			{
				return 0x0000;
			}
		}
	}
	
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
	
	sendRequest(request);
	free((void*)request);

	char* answer; // The pointer that will point to the answer
	waitForAnswer(id,&answer);
	// when the function terminates we are sure to have received a message


	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_PROFILE *abody=(ANSWER_CAPI_PROFILE*)(answer+aheader->header_len);
	char* adata=answer+aheader->header_len+aheader->body_len;

	if(!verifyMessage(answer,TYPE_CAPI_PROFILE))
	{
        free((void*)answer);
		return 0x1108;
	}	

	memcpy(szBuffer,abody->profile,64);		// copy the profile to the result
	*((UINT*)szBuffer)=controllers;		// Set the number of returned controllers to the real number of controller we got
	
	
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
	//first try to call CAPI_INSTALLED from oldcapi2032.dll
	if(old_installed!=NULL)
	{
		if(old_installed()==0x0000) // zero means NO_ERROR -> CAPI is installed
			return 0x0000;
	}

	
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
	
	sendRequest(request);
	free((void*)request);

	char* answer; // The pointer that will point to the answer
	waitForAnswer(id,&answer);
	// when the function terminates we are sure to have received a message


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
