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
 * Revision 1.5  2002/03/03 20:38:19  butzist
 * added Log history
 *
 */

/*
    Main source file for the server
*/

#include "stdafx.h"
#include "capi20server.h"
#include "protocol.h"

#define __PORT	6674	// Fritzle's Telefonnummer
#define _MAX_SESSIONS	127
#define _MSG_SIZE		10000

const char* LOGFILE="c:\\winnt\\capi20proxy.log";

SERVICE_STATUS          ServiceStatus; 
SERVICE_STATUS_HANDLE   ServiceStatusHandle; 
int run,pause,debug,recv_threads; 
FILE *f;
HANDLE RunningThread;
DWORD session=0;

HANDLE Router;

void DebugOut(char* msg,BOOL type_error){
	DWORD err=GetLastError();
	if(f!=NULL){
		fprintf(f,"%s",msg);
		if(type_error) fprintf(f," \tError: %lu",err);
		fprintf(f,"\r\n");
		fflush(f);
	} else {
		printf("%s",msg);
		if(type_error) printf(" \tError: %lu",err);
		printf("\r\n");
	}
}


int main(int argc, char* argv[])
{ 
	RunningThread=0;
	debug=0;

	for(int i=1;i<argc;i++) {
		if(strcmp(argv[i],"-debug")==0){
			debug=1; // Debug Modus!
			f=NULL;
		}
	}
	
	// open file for debug output
	if(debug==0) f=fopen(LOGFILE,"a+t");

	for(i=1;i<argc;i++) {
		if(strcmp(argv[i],"-i")==0){
			SC_HANDLE schSCManager = OpenSCManager( 
				NULL,                    // local machine 
				NULL,                    // ServicesActive database 
				SC_MANAGER_ALL_ACCESS);  // full access rights 

			if (schSCManager == NULL) 
				DebugOut("main(): OpenSCManager");

			
			char lpszBinaryPathName[512];
			GetModuleFileName(NULL,lpszBinaryPathName,512);

			SC_HANDLE schService = CreateService( 
				schSCManager,              // SCManager database 
				"capi20server",            // name of service 
				"CAPI 2.0 Proxy Server",  // service name to display 
				SERVICE_ALL_ACCESS,        // desired access 
				SERVICE_WIN32_OWN_PROCESS, // service type 
				SERVICE_AUTO_START,        // start type 
				SERVICE_ERROR_NORMAL,      // error control type 
				lpszBinaryPathName,        // service's binary 
				NULL,                      // load ordering group 
				NULL,                      // tag identifier 
				NULL,				       // dependencies 
				NULL,                      // LocalSystem account 
				NULL);                     // password 

			if (schService == NULL) { 
				DebugOut("main(): CreateService");
				printf("CreateService FAILED\r\n");
				return 1;
			} else {
				printf("CreateService SUCCESS.\r\n");
				return 0;
			}

			CloseServiceHandle(schService);
		} else

		if(strcmp(argv[i],"-u")==0){
			SC_HANDLE schSCManager = OpenSCManager( 
				NULL,                    // local machine 
				NULL,                    // ServicesActive database 
				SC_MANAGER_ALL_ACCESS);  // full access rights 

			if (schSCManager == NULL) 
				DebugOut("main(): OpenSCManager");

			SC_HANDLE schService = OpenService( 
				schSCManager,       // SCManager database 
				"capi20server",       // name of service 
				DELETE);            // only need DELETE access 

			if (schService == NULL) 
				DebugOut("main(): OpenService"); 

			if (! DeleteService(schService) ) {
				DebugOut("main(): DeleteService"); 
				printf("DeleteService FAILED\r\n");
				return 1;
			} else {
				printf("DeleteService SUCCESS\r\n");
				return 0;
			}

			CloseServiceHandle(schService); 
		} else

		if((strcmp(argv[i],"-?")==0) || (strcmp(argv[i],"-help")==0)){
			printf("\r\nUse folloging switches:\r\n\t-i\tInstall as Service under Windows NT\r\n\t-u\tUninstall the Service\r\n\t-debug\tRun Debug mode. Works also under Windows 9.x\r\n");
			return 0;
		}
	}


	//Startup Windows Socket Architecture
	
	WSADATA wsadata;
	if(WSAStartup(MAKEWORD(2,2),&wsadata)!=0){
		DebugOut("main(): WSAStartup");
		return 1;
	}

	
	// The Debug mode starts the server in a console (not as a service in 
	// the background). All messages are sent to the console.
	if(!debug){
		SERVICE_TABLE_ENTRY   DispatchTable[] = 
		{ 
			{ "capi20server", ServiceStart      }, 
			{ NULL,              NULL          } 
		}; 
 
		if (!StartServiceCtrlDispatcher(DispatchTable)) 
		{ 
			//Error
			DebugOut("main(): StartServiceCtrlDispatcher");
		} 
	} else {
		DWORD thid;
		
		RunningThread=CreateThread(NULL,0,TestStart,NULL,0,&thid);
		if(RunningThread==NULL){
			DebugOut("main(): CreateThread");
		} else {
			::MessageBox(NULL,"\"OK\" to shutdown the server","Capi 2.0 Proxy Server",MB_OK);
			ServiceExit();
		}
	}

	DebugOut("Exiting main()",FALSE);
	WSACleanup();
	_fcloseall();
	return 0;
}


DWORD WINAPI TestStart(LPVOID param){
	ServiceStart(0,NULL);
	return 0;
}

void WINAPI ServiceStart (DWORD argc, LPTSTR *argv) 
{ 
	DWORD status; 
	DWORD specificError; 

    if(!debug){
		ServiceStatus.dwServiceType        = SERVICE_WIN32; 
		ServiceStatus.dwCurrentState       = SERVICE_START_PENDING; 
		ServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP | 
			SERVICE_ACCEPT_PAUSE_CONTINUE; 
		ServiceStatus.dwWin32ExitCode      = 0; 
		ServiceStatus.dwServiceSpecificExitCode = 0; 
		ServiceStatus.dwCheckPoint         = 0; 
		ServiceStatus.dwWaitHint           = 0; 
	}
 
 	run=1;
	pause=0;

	if(!debug){
		ServiceStatusHandle = RegisterServiceCtrlHandler("capi20server",&ServiceCtrlHandler); 
 
		if (ServiceStatusHandle == (SERVICE_STATUS_HANDLE)0) 
		{ 
			DebugOut("ServiceMain(): RegisterServiceCtrlHandler");
			return; 
		}
	}
 
    // Initialization code goes here. 
    status = ServiceInitialization(argc,argv, &specificError); 
 
	// Handle error condition 
    if (status != 0) 
    { 
	    if(!debug){
			ServiceStatus.dwCurrentState       = SERVICE_STOPPED; 
			ServiceStatus.dwCheckPoint         = 0; 
			ServiceStatus.dwWaitHint           = 0; 
			ServiceStatus.dwWin32ExitCode      = status; 
			ServiceStatus.dwServiceSpecificExitCode = specificError; 
 
			SetServiceStatus (ServiceStatusHandle, &ServiceStatus); 
		}
        return; 
    } 
 
    
	if(!debug){
		// Initialization complete - report running status. 
		ServiceStatus.dwCurrentState       = SERVICE_RUNNING; 
		ServiceStatus.dwCheckPoint         = 0; 
		ServiceStatus.dwWaitHint           = 0; 
 
		if (!SetServiceStatus (ServiceStatusHandle, &ServiceStatus)) 
		{ 
			DebugOut("ServiceMain(): SetServiceStatus");
			return;
		} 
	}

	DebugOut("Server running...",FALSE);
 
	while(run==1){
		Sleep(100);
		if(pause!=1){
			fd_set test;
			test.fd_count=1;
			test.fd_array[0]=socke;
			
			timeval time;
			time.tv_sec=0;
			time.tv_usec=500;

			if(select(0,&test,NULL,NULL,&time)==SOCKET_ERROR){
				DebugOut("ServiceMain(): select",FALSE);
				return;
			}

			if(test.fd_count!=0){
				DWORD thid;
				HANDLE threadhandle;
				SOCKET connection=accept(socke,NULL,0);
				if(connection==INVALID_SOCKET)
				{
					DebugOut("ServiceMain(): accept",FALSE);
					continue;
				}

				DWORD sess=allocSession(socke);
				if(sess==0)
				{
					DebugOut("ServiceMain(): no free session Ids",FALSE);
					continue;
				}

				if((threadhandle=CreateThread(NULL,0,StartSession,(LPVOID)sess,0,&thid))==NULL){
					DebugOut("ServiceMain(): CreateThread");
				} else {
					CloseHandle(threadhandle);
				}
			}
		}
	}
	
	//Exiting
	DebugOut("Exiting ServiceMain()",FALSE);
   return; 
} 

DWORD WINAPI StartSession(LPVOID param)
{
	char* request=NULL;
	sess=(DWORD)param;
	DWORD err=1;
	char* request;
	SOCKET socke=sockets[sess];
	LPVOID auth_data;
	UINT auth_type;
	UINT auth_len;
	char name[64];

	do {
		if(pause!=1)
		{
			request=malloc(_MSG_SIZE);
			if(request==NULL)
			{
				DebugOut("StartSession(): malloc");
				break;	// or perhaps just continue
			}

			err=recv(socke,request,_MSG_SIZE,0);	// blocking call
			if(err==0 || err==SOCKET_ERROR)
			{
				free((void*)request);
				break;
			}

			verifyRequest(request);		// verify sizes
			verifySessionId(request,session+sess);	// verify session_id

			if(debug) DebugOut("ReceiverRequest(): Received Request",FALSE);

			REQUEST_HEADER *header=(REQUEST_HEADER*)request;
			LPVOID body=(LPVOID)(request+header->header_len);
			char* data=request+header->header_len+header->body_len;

			switch(rheader->message_type)
			{
			case TYPE_CAPI_REGISTER:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_REGISTER",FALSE);
				exec_capi_register(socke,header,(REQUEST_CAPI_REGISTER*)body,data);
				break;

			case TYPE_CAPI_RELEASE:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_RELEASE",FALSE);
				exec_capi_release(socke,header,(REQUEST_CAPI_RELEASE*)body,data);
				break;

			case TYPE_CAPI_PUTMESSAGE:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_PUTMESSAGE",FALSE);
				exec_capi_putmessage(socke,header,(REQUEST_CAPI_PUTMESSAGE*)body,data);
				break;

			case TYPE_CAPI_GETMESSAGE:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_GETMESSAGE",FALSE);
				exec_capi_getmessage(socke,header,(REQUEST_CAPI_GETMESSAGE*)body,data);
				break;

			case TYPE_CAPI_WAITFORSIGNAL:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_WAITFORSIGNAL",FALSE);
				exec_capi_waitforsignal(socke,header,(REQUEST_CAPI_WAITFORSIGNAL*)body,data);
				break;

			case TYPE_CAPI_MANUFACTURER:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_MANUFACTURER",FALSE);
				exec_capi_manufacturer(socke,header,(REQUEST_CAPI_MANUFACTURER*)body,data);
				break;

			case TYPE_CAPI_VERSION:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_VERSION",FALSE);
				exec_capi_version(socke,header,(REQUEST_CAPI_VERSION*)body,data);
				break;

			case TYPE_CAPI_SERIAL:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_SERIAL",FALSE);
				exec_capi_serial(socke,header,(REQUEST_CAPI_SERIAL*)body,data);
				break;

			case TYPE_CAPI_PROFILE:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_PROFILE",FALSE);
				exec_capi_profile(socke,header,(REQUEST_CAPI_PROFILE*)body,data);
				break;

			case TYPE_CAPI_INSTALLED:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_INSTALLED",FALSE);
				exec_capi_installed(socke,header,(REQUEST_CAPI_INSTALLED*)body,data);
				break;

			case TYPE_PROXY_HELO:
				if(debug) DebugOut("ReceiveRequest(): TYPE_PROXY_HELO",FALSE);
				exec_proxy_helo(socke,header,(REQUEST_PROXY_HELO*)body,data);
				break;

			case TYPE_PROXY_KEEPALIVE:
				if(debug) DebugOut("ReceiveRequest(): TYPE_PROXY_KEEPALIVE",FALSE);
				exec_proxy_keepaplive(socke,header,(REQUEST_PROXY_KEEPALIVE*)body,data);
				break;

			case TYPE_PROXY_AUTH:
				if(debug) DebugOut("ReceiveRequest(): TYPE_PROXY_AUTH",FALSE);
				exec_proxy_auth(socke,header,(REQUEST_PROXY_AUTH*)body,data);
				break;

			default:
				DebugOut("ReceiveRequest(): Invalid Message Type",FALSE);
			}
			free((void*)request);
		}
	}

	if(err==0)
	{
		send_answer_proxy_shutdown(socke,session+sess,"Connection shutting down");
	} else {
		send_answer_proxy_shutdown(socke,session+sess,"Socket Error");
	}

	closesocket(socke);
	freeSession(sess);
}	
 
// Stub initialization function. 
DWORD ServiceInitialization(DWORD argc, LPTSTR *argv,DWORD *specificError) 
{
	session+=0x10000;
	socke=CreateSocket(SOCK_STREAM,IPPROTO_TCP,__PORT);
	if(socke==INVALID_SOCKET){
		DWORD err=GetLastError();
		SetLastError(err);
		DebugOut("InitService(): CreateSocket");
		return err;
	}

	listen(socke,10);

	return NO_ERROR;
} 

VOID WINAPI ServiceCtrlHandler (DWORD Opcode) 
{ 
    switch(Opcode) 
    { 
        case SERVICE_CONTROL_PAUSE: 
            pause=1;
			ServiceStatus.dwCurrentState = SERVICE_PAUSED; 
            break; 
 
        case SERVICE_CONTROL_CONTINUE: 
			pause=0;
			ServiceStatus.dwCurrentState = SERVICE_RUNNING; 
            break; 
 
        case SERVICE_CONTROL_STOP: 
			ServiceExit();

			ServiceStatus.dwWin32ExitCode = 0; 
            ServiceStatus.dwCurrentState  = SERVICE_STOPPED; 
            ServiceStatus.dwCheckPoint    = 0; 
            ServiceStatus.dwWaitHint      = 0; 
 
            if (!SetServiceStatus (ServiceStatusHandle, 
                &ServiceStatus))
            { 
				//Error
				DebugOut("ServiceCtrlHandler(): SetServiceStatus");
            } 

			//Leaving Service
			DebugOut("ServiceCtrlHandler(): Leaving Service",FALSE); 
            return; 
 
        case SERVICE_CONTROL_INTERROGATE: 
        // Fall through to send current status. 
            break; 
 
        default: 
			//Unrecognized Opcode
			DebugOut("ServiceCtrlHandler(): Unrecognized Opcode",FALSE);
    } 
 
    // Send current status. 
    if (!SetServiceStatus (ServiceStatusHandle,  &ServiceStatus)) 
    { 
		//Error
		DebugOut("ServiceCtrlHandler(): SetServiceStatus");
    } 
    return; 
} 

DWORD WINAPI Wait(LPVOID params){
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
	IN_ADDR localaddr={INADDR_ANY};
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

DWORD ServiceExit(){
    run=0;
	int stopwait=0; // for stopping the wait queues. will be set by a thread to 1
                    // after a specified time.

	if(WaitForSingleObject(RunningThread,5000)!=WAIT_OBJECT_0){
		DebugOut("ExitThread(): WaitForSingleObject");
		TerminateThread(RunningThread,ERROR_BUSY);
	}
	CloseHandle(RunningThread);


	if(TimeOut(30000,&stopwait,1)){
		while((!stopwait) && recv_threads){
			Sleep(100);
		}
	}
	
	if(recv_threads){
		SetLastError(recv_threads);
		DebugOut("ExitService(): Some threads hung! Closing socket and waiting...");
	}
		
	//shutdown(socke,SD_BOTH);
	closesocket(socke);

	stopwait=0;
	
	if(TimeOut(5000,&stopwait,1)){
		while((!stopwait) && recv_threads){
			Sleep(100);
		}
	}

	if(recv_threads){
		SetLastError(recv_threads);
		DebugOut("ExitService(): Sorry but still some threads aure hung. Exiting anyways.");
	}

	return (DWORD)recv_threads;
}

