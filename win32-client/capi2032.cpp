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
    main source for capi2032.dll
*/

// capi2032.cpp : Definiert den Einsprungpunkt für die DLL-Anwendung.
//

#include "stdafx.h"
#include "protocol.h"

struct TOParams{
	DWORD Milliseconds;
	int* var;
	int val;
};

LPVOID GlobalBuffer1=NULL;
LPVOID GlobalBuffer2=NULL;

IN_ADDR toaddr;
IN_ADDR localaddr;

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

		len=256;
		err=RegQueryValueEx(key,"localaddr",NULL,NULL,(unsigned char*)ipstr,&len);
		if(err!=ERROR_SUCCESS){
			::MessageBox(NULL,"Could not read Regitry Value \"HKEY_LOCAL_MACHINE\\Software\\The Reg Guy\\capi20proxy\\localaddr\"","Error",MB_OK);
			return FALSE;
		}
		localaddr.S_un.S_addr=inet_addr(ipstr);

		RegCloseKey(key);
		
		break;
 
	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:

		if(GlobalBuffer1!=NULL) GlobalFree(GlobalBuffer1);
		if(GlobalBuffer2!=NULL) GlobalFree(GlobalBuffer2);

		WSACleanup();
		break;
	}
	return TRUE;
}

SOCKET CreateSocket(int socktype, int protocol, UINT port)
{
	SOCKET sock;
	if((sock=socket(AF_INET,socktype,protocol))==INVALID_SOCKET){
		DWORD err=WSAGetLastError();
		SetLastError(err);
		return INVALID_SOCKET;
	} 

	SOCKADDR_IN local;
	local.sin_family=AF_INET;
	local.sin_addr=localaddr;
	local.sin_port=htons(port);

	if(bind(sock,(SOCKADDR*)&local,sizeof(SOCKADDR_IN))==SOCKET_ERROR){
		DWORD err=WSAGetLastError();
		SetLastError(err);
		return INVALID_SOCKET;
	}
	return sock;
}

/*DWORD WINAPI Wait(LPVOID params){
	TOParams *to=(TOParams*)params;
	
	Sleep(to->Milliseconds);
	
	*(to->var)=to->val;

	return 0;
}

HANDLE TimeOut(DWORD Milliseconds,int* var,int val){
	DWORD thid;
	TOParams param;
	param.Milliseconds=Milliseconds;
	param.var=var;
	param.val=val;

	return CreateThread(NULL,0,Wait,(LPVOID)&param,0,&thid);
}*/

int SocketSendTo(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen, DWORD timeout)
{
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
		return sendto(s,buf,len,flags,to,tolen);
	}
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

int SocketReceiveFrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen, DWORD timeout){
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
		return recvfrom(s,buf,len,flags,from,fromlen);
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

SOCKET SocketAccept(SOCKET s, struct sockaddr *addr, int *addrlen, DWORD timeout){
	fd_set sockets;
	timeval select_timeout;
	int ret;
	
	select_timeout.tv_sec=timeout;
	select_timeout.tv_usec=0;

	FD_ZERO(&sockets);
	FD_SET(s,&sockets);
	ret=select(0,&sockets,NULL,NULL,&select_timeout);

	if((ret==0) || (ret==SOCKET_ERROR)){
		return INVALID_SOCKET;
	} else {
		return accept(s,addr,addrlen);
	}
}

// Here come the exported CAPI 2.0 Functions

DWORD APIENTRY CAPI_REGISTER(DWORD MessageBufferSize,
						   DWORD maxLogicalConnection,
						   DWORD maxBDataBlocks,
						   DWORD maxBDataLen,
						   DWORD *pApplID)
{
	char buffer[512];
	DWORD err;
	SOCKET socke=CreateSocket(SOCK_DGRAM,IPPROTO_UDP,0);
	if(socke==INVALID_SOCKET){
		return 0x1008;
	}

	SOCKADDR_IN to;
	
	to.sin_addr=toaddr;
	to.sin_family=AF_INET;
	to.sin_port=htons(7103);

	ANSWER *answer;
	REQUEST request;
	request.data.rd_register.MessageBufferSize=MessageBufferSize;
	request.data.rd_register.maxLogicalConnection=maxLogicalConnection;
	request.data.rd_register.maxBDataBlocks=maxBDataBlocks;
	request.data.rd_register.maxBDataLen=maxBDataLen;
	request.type=TYPE_REGISTER;

	err=SocketSendTo(socke,(LPCTSTR)&request,sizeof(REQUEST),0,(SOCKADDR*)&to,sizeof(SOCKADDR_IN),2);
	if(err==SOCKET_ERROR){
		closesocket(socke);
		return 0x1008;
	}
	
	err=SocketReceiveFrom(socke,buffer,512,0,NULL,NULL,10);
	if(err==SOCKET_ERROR){
		closesocket(socke);
		return 0x1008;
	}
	
	err=closesocket(socke);
	if(err==SOCKET_ERROR){
		return 0x1008;
	}

	answer=(ANSWER*)buffer;

	*pApplID=answer->data.ad_register.ApplID;

	return answer->data.ad_register.ret;
}

DWORD APIENTRY CAPI_RELEASE(DWORD ApplID)
{
	char buffer[512];
	DWORD err;
	SOCKET socke=CreateSocket(SOCK_DGRAM,IPPROTO_UDP,0);
	if(socke==INVALID_SOCKET){
		return 0x1108;
	}

	SOCKADDR_IN to;
	
	to.sin_addr=toaddr;
	to.sin_family=AF_INET;
	to.sin_port=htons(7103);

	ANSWER *answer;
	REQUEST request;
	request.data.rd_release.ApplID=ApplID;
	request.type=TYPE_RELEASE;

	err=SocketSendTo(socke,(LPCTSTR)&request,sizeof(REQUEST),0,(SOCKADDR*)&to,sizeof(SOCKADDR_IN),2);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return 0x1108;
	}

	err=SocketReceiveFrom(socke,buffer,512,0,NULL,NULL,10);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return 0x1108;
	}

	err=closesocket(socke);
	if(err==SOCKET_ERROR){
		return 0x1108;
	}

	answer=(ANSWER*)buffer;
	
	return answer->data.ad_release.ret;
}

DWORD APIENTRY CAPI_PUT_MESSAGE(DWORD ApplID,
							  LPVOID pCAPIMessage)
{
	char buffer[512];
	DWORD err;
	char *data1,*data2;
	UINT len1=0,len2=0;

	int len;
	SOCKET socke=CreateSocket(SOCK_DGRAM,IPPROTO_UDP,0);
	if(socke==INVALID_SOCKET){
		return 0x1108;
	}
	SOCKADDR_IN to,clientaddr,me;
	
	to.sin_addr=toaddr;
	to.sin_family=AF_INET;
	to.sin_port=htons(7103);

	SOCKET server=CreateSocket(SOCK_STREAM,IPPROTO_TCP,0);
	if(server==INVALID_SOCKET){
		closesocket(socke);
		return 0x1108;
	}
	err=listen(server,5);
	if(err==SOCKET_ERROR){
		closesocket(server);
		closesocket(socke);
		return 0x1108;
	}

	ANSWER *answer;
	REQUEST request;
	request.data.rd_put_message.ApplID=ApplID;
	
	len=sizeof(SOCKADDR_IN);
	err=getsockname(server,(SOCKADDR*)&me,&len);
	if(err==SOCKET_ERROR){
		closesocket(server);
		closesocket(socke);
		return 0x1108;
	}

	request.data.rd_put_message.me=me;
	request.type=TYPE_PUT_MESSAGE;

	err=SocketSendTo(socke,(LPCTSTR)&request,sizeof(REQUEST),0,(SOCKADDR*)&to,sizeof(SOCKADDR_IN),2);
	if(err==SOCKET_ERROR){
		closesocket(server);
		closesocket(socke);
		return 0x1108;
	}

	data1=(char*)pCAPIMessage;
	memcpy(&len1,data1,2);

	SOCKET connection=SocketAccept(server,(SOCKADDR*)&clientaddr,&len,120);
	if(connection==INVALID_SOCKET){
		closesocket(server);
		closesocket(socke);
		return 0x1108;
	}

	//probably later
	err=closesocket(server);
	if(err==SOCKET_ERROR){
		closesocket(socke);
		return 0x1108;
	}


	err=SocketSend(connection,data1,2,0,2);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(connection);
		closesocket(server);
		closesocket(socke);
		return 0x1108;
	}
	
	err=SocketSend(connection,data1+2,len1-2,0,2);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(connection);
		closesocket(server);
		closesocket(socke);
		return 0x1108;
	}
	
	/*err=closesocket(connection);
	if(err==SOCKET_ERROR){
		closesocket(server);
		closesocket(socke);
		return 0x1108;
	}*/

	unsigned char cmd1=data1[4];
	unsigned char cmd2=data1[5];

	if((cmd1==0x86) && (cmd2==0x80)){
		/*connection=accept(server,(SOCKADDR*)&clientaddr,&len);
		if(connection==INVALID_SOCKET){
			closesocket(server);
			closesocket(socke);
			return 0x1108;
		}*/
		
		memcpy(&len2,data1+16,2);
		DWORD pointer;
		memcpy(&pointer,data1+12,4);
		data2=(char*)(LPVOID)pointer;

		err=SocketSend(connection,data2,len2,0,2);
		if((err==SOCKET_ERROR) || (err==0)){
			closesocket(connection);
			closesocket(server);
			closesocket(socke);
			return 0x1108;
		}
		
	}

	err=closesocket(connection);
	if(err==SOCKET_ERROR){
		closesocket(socke);
		return 0x1108;
	}

	err=SocketReceiveFrom(socke,buffer,512,0,NULL,NULL,10);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return 0x1108;
	}
	
	err=closesocket(socke);
	if(err==SOCKET_ERROR){
		return 0x1108;
	}

	answer=(ANSWER*)buffer;
	
	return answer->data.ad_put_message.ret;
}

DWORD APIENTRY CAPI_GET_MESSAGE(DWORD ApplID, LPVOID *ppCAPIMessage)
{
	char buffer[512];
	DWORD err;
	UINT len1=0,len2=0;

	int len;
	SOCKET socke=CreateSocket(SOCK_DGRAM,IPPROTO_UDP,0);
	if(socke==INVALID_SOCKET){
		return 0x1108;
	}

	SOCKADDR_IN to,clientaddr,me;
	
	to.sin_addr=toaddr;
	to.sin_family=AF_INET;
	to.sin_port=htons(7103);

	SOCKET server=CreateSocket(SOCK_STREAM,IPPROTO_TCP,0);
	if(server==INVALID_SOCKET){
		closesocket(socke);
		return 0x1108;
	}
	
	err=listen(server,2);
	if(err==SOCKET_ERROR){
		closesocket(server);
		closesocket(socke);
		return 0x1108;
	}

	ANSWER *answer;
	REQUEST request;
	request.data.rd_get_message.ApplID=ApplID;
	
	len=sizeof(SOCKADDR_IN);
	err=getsockname(server,(SOCKADDR*)&me,&len);
	if(err==SOCKET_ERROR){
		closesocket(server);
		closesocket(socke);
		return 0x1108;
	}
	
	request.data.rd_get_message.me=me;
	request.type=TYPE_GET_MESSAGE;

	err=SocketSendTo(socke,(LPCTSTR)&request,sizeof(REQUEST),0,(SOCKADDR*)&to,sizeof(SOCKADDR_IN),2);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(server);
		closesocket(socke);
		return 0x1108;
	}

	
	SOCKET connection=SocketAccept(server,(SOCKADDR*)&clientaddr,&len,120);
	if(connection==INVALID_SOCKET){
		closesocket(server);
		closesocket(socke);
		return 0x1108;
	}

	//probably also later
	err=closesocket(server);
	if(err==SOCKET_ERROR){
		closesocket(socke);
		return 0x1108;
	}

	err=SocketReceive(connection,(char*)&len1,2,0,10);
	if((err==SOCKET_ERROR) || (err<2) || (len1<8)){
		closesocket(connection);
		closesocket(server);
		closesocket(socke);
		return 0x1108;
	}

	if(GlobalBuffer1!=NULL) GlobalFree(GlobalBuffer1);
	GlobalBuffer1=GlobalAlloc(GPTR,len1);
	if(GlobalBuffer1==NULL){
		closesocket(connection);
		closesocket(server);
		closesocket(socke);
		return 0x1108;
	}

	
	memcpy(GlobalBuffer1,&len1,2);
	err=SocketReceive(connection,((char*)GlobalBuffer1+2),len1-2,0,10);
	if((err==SOCKET_ERROR) || (err<(len1-2))){
		closesocket(connection);
		closesocket(server);
		closesocket(socke);
		return 0x1108;
	}
	
	/*err=closesocket(connection);
	if(err==SOCKET_ERROR){
		closesocket(server);
		closesocket(socke);
		return 0x1108;
	}*/

	unsigned char cmd1=((char*)GlobalBuffer1)[4];
	unsigned char cmd2=((char*)GlobalBuffer1)[5];

	if((cmd1==0x86) && (cmd2==0x82)){
		if(len1<18){
			closesocket(server);
			closesocket(socke);
			return 0x1108;
		}
		
		/*connection=accept(server,(SOCKADDR*)&clientaddr,&len);
		if(connection==INVALID_SOCKET){
			closesocket(server);
			closesocket(socke);
			return 0x1108;
		}*/
		
		memcpy(&len2,((char*)GlobalBuffer1)+16,2);
		if(GlobalBuffer2!=NULL) GlobalFree(GlobalBuffer2);
		GlobalBuffer2=GlobalAlloc(GPTR,len2);
		if(GlobalBuffer2==NULL){
			closesocket(connection);
			closesocket(server);
			closesocket(socke);
			return 0x1108;
		}
		
		err=SocketReceive(connection,((char*)GlobalBuffer2),len2,0,10);
		if((err==SOCKET_ERROR) ||  (err==0) || (err<len2)){
			closesocket(connection);
			closesocket(server);
			closesocket(socke);
			return 0x1108;
		}

		
		DWORD pointer;
		pointer=(DWORD)GlobalBuffer2;
		if(pointer==0){
			closesocket(connection);
			closesocket(server);
			closesocket(socke);
			return 0x1108;
		}
		memcpy(((char*)GlobalBuffer1)+12,&pointer,4);

	}
	
	err=closesocket(connection);
	if(err==SOCKET_ERROR){
		closesocket(socke);
		return 0x1108;
	}

	*ppCAPIMessage=GlobalBuffer1;

	err=SocketReceiveFrom(socke,buffer,512,0,NULL,NULL,10);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return 0x1108;
	}

	err=closesocket(socke);
	if(err==SOCKET_ERROR){
		return 0x1108;
	}

	answer=(ANSWER*)buffer;
	
	return answer->data.ad_get_message.ret;
}

DWORD APIENTRY CAPI_WAIT_FOR_SIGNAL(DWORD ApplID)
{
	DWORD err;
	char buffer[512];
	
	SOCKET socke=CreateSocket(SOCK_DGRAM,IPPROTO_UDP,0);
	if(socke==INVALID_SOCKET){
		return 0x1108;
	}
	
	SOCKADDR_IN to;
	
	to.sin_addr=toaddr;
	to.sin_family=AF_INET;
	to.sin_port=htons(7103);

	ANSWER *answer;
	REQUEST request;
	request.data.rd_wait_for_signal.ApplID=ApplID;
	request.type=TYPE_WAIT_FOR_SIGNAL;

	err=SocketSendTo(socke,(LPCTSTR)&request,sizeof(REQUEST),0,(SOCKADDR*)&to,sizeof(SOCKADDR_IN),2);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return 0x1108;
	}

	err=SocketReceiveFrom(socke,buffer,512,0,NULL,NULL,180);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return 0x1108;
	}

	err=closesocket(socke);
	if(err==SOCKET_ERROR){
		return 0x1108;
	}

	answer=(ANSWER*)buffer;
	
	return answer->data.ad_wait_for_signal.ret;
}

void APIENTRY CAPI_GET_MANUFACTURER(char* szBuffer)
{
	szBuffer[0]=0;

	char buffer[512];
	DWORD err;
	SOCKET socke=CreateSocket(SOCK_DGRAM,IPPROTO_UDP,0);
	if(socke==INVALID_SOCKET){
		return;
	}

	SOCKADDR_IN to;
	
	to.sin_addr=toaddr;
	to.sin_family=AF_INET;
	to.sin_port=htons(7103);

	ANSWER *answer;
	REQUEST request;
	request.type=TYPE_GET_MANUFACTURER;

	err=SocketSendTo(socke,(LPCTSTR)&request,sizeof(REQUEST),0,(SOCKADDR*)&to,sizeof(SOCKADDR_IN),2);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return;
	}
	
	err=SocketReceiveFrom(socke,buffer,512,0,NULL,NULL,10);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return;
	}

	err=closesocket(socke);
	if(err==SOCKET_ERROR){
		return;
	}

	answer=(ANSWER*)buffer;

	memcpy(szBuffer,answer->data.ad_get_manufacturer.szBuffer,64);
	
//	return answer->data.ad_get_manufacturer.ret;
}

DWORD APIENTRY CAPI_GET_VERSION(DWORD * pCAPIMajor,
							  DWORD * pCAPIMinor,
							  DWORD * pManufacturerMajor,
							  DWORD * pManufacturerMinor)
{
	char buffer[512];
	DWORD err;
	SOCKET socke=CreateSocket(SOCK_DGRAM,IPPROTO_UDP,0);
	if(socke==INVALID_SOCKET){
		return 0x1108;
	}

	SOCKADDR_IN to;
	
	to.sin_addr=toaddr;
	to.sin_family=AF_INET;
	to.sin_port=htons(7103);

	ANSWER *answer;
	REQUEST request;
	request.type=TYPE_GET_VERSION;

	err=SocketSendTo(socke,(LPCTSTR)&request,sizeof(REQUEST),0,(SOCKADDR*)&to,sizeof(SOCKADDR_IN),2);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return 0x1108;
	}

	err=SocketReceiveFrom(socke,buffer,512,0,NULL,NULL,10);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return 0x1108;
	}

	err=closesocket(socke);
	if(err==SOCKET_ERROR){
		closesocket(socke);
		return 0x1108;
	}

	answer=(ANSWER*)buffer;

	*pCAPIMajor=answer->data.ad_get_version.CAPIMajor;
	*pCAPIMinor=answer->data.ad_get_version.CAPIMinor;
	*pManufacturerMajor=answer->data.ad_get_version.ManufacturerMajor;
	*pManufacturerMinor=answer->data.ad_get_version.MAnufacturerMinor;
	
	return answer->data.ad_get_version.ret;
}

DWORD APIENTRY CAPI_GET_SERIAL_NUMBER(char* szBuffer)
{
	szBuffer[0]=0;

	char buffer[512];
	DWORD err;
	SOCKET socke=CreateSocket(SOCK_DGRAM,IPPROTO_UDP,0);
	if(socke==INVALID_SOCKET){
		return 0x1108;
	}

	SOCKADDR_IN to;
	
	to.sin_addr=toaddr;
	to.sin_family=AF_INET;
	to.sin_port=htons(7103);

	ANSWER *answer;
	REQUEST request;
	request.type=TYPE_GET_SERIAL_NUMBER;

	err=SocketSendTo(socke,(LPCTSTR)&request,sizeof(REQUEST),0,(SOCKADDR*)&to,sizeof(SOCKADDR_IN),2);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return 0x1108;
	}

	err=SocketReceiveFrom(socke,buffer,512,0,NULL,NULL,10);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return 0x1108;
	}

	err=closesocket(socke);
	if(err==SOCKET_ERROR){
		return 0x1108;
	}

	answer=(ANSWER*)buffer;

	memcpy(szBuffer,answer->data.ad_get_serial_number.szBuffer,8);
	
	return answer->data.ad_get_serial_number.ret;
}

DWORD APIENTRY CAPI_GET_PROFILE(LPVOID szBuffer,
								DWORD CtrlNr)
{
	((char*)szBuffer)[0]=0;
	
	char buffer[512];
	DWORD err;
	SOCKET socke=CreateSocket(SOCK_DGRAM,IPPROTO_UDP,0);
	if(socke==INVALID_SOCKET){
		return 0x1108;
	}

	SOCKADDR_IN to;
	
	to.sin_addr=toaddr;
	to.sin_family=AF_INET;
	to.sin_port=htons(7103);

	ANSWER *answer;
	REQUEST request;
	request.data.rd_get_profile.CtrlNr=CtrlNr;
	request.type=TYPE_GET_PROFILE;

	err=SocketSendTo(socke,(LPCTSTR)&request,sizeof(REQUEST),0,(SOCKADDR*)&to,sizeof(SOCKADDR_IN),2);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return 0x1108;
	}

	err=SocketReceiveFrom(socke,buffer,512,0,NULL,NULL,10);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return 0x1108;
	}

	err=closesocket(socke);
	if(err==SOCKET_ERROR){
		return 0x1108;
	}

	answer=(ANSWER*)buffer;

	memcpy(szBuffer,answer->data.ad_get_profile.szBuffer,64);
	
	return answer->data.ad_get_profile.ret;
}

DWORD APIENTRY CAPI_INSTALLED(void)
{
	char buffer[512];
	DWORD err;
	SOCKET socke=CreateSocket(SOCK_DGRAM,IPPROTO_UDP,0);
	if(socke==INVALID_SOCKET){
		return 0x1108;
	}

	SOCKADDR_IN to;
	
	to.sin_addr=toaddr;
	to.sin_family=AF_INET;
	to.sin_port=htons(7103);

	ANSWER *answer;
	REQUEST request;
	request.type=TYPE_INSTALLED;

	err=SocketSendTo(socke,(LPCTSTR)&request,sizeof(REQUEST),0,(SOCKADDR*)&to,sizeof(SOCKADDR_IN),2);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return 0x1108;
	}

	err=SocketReceiveFrom(socke,buffer,512,0,NULL,NULL,10);
	if((err==SOCKET_ERROR) || (err==0)){
		closesocket(socke);
		return 0x1108;
	}

	err=closesocket(socke);
	if(err==SOCKET_ERROR){
		return 0x1108;
	}

	answer=(ANSWER*)buffer;
	
	return answer->data.ad_installed.ret;
}
