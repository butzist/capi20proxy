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
    Main source file for the server
*/

#include "stdafx.h"
#include "capi20server.h"
#include "protocol.h"


SERVICE_STATUS          ServiceStatus; 
SERVICE_STATUS_HANDLE   ServiceStatusHandle; 
int run,pause,debug,recv_threads; 
FILE *f;
SOCKET socke;
HANDLE RunningThread;

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
	recv_threads=0;

	for(int i=1;i<argc;i++) {
		if(strcmp(argv[i],"-debug")==0){
			debug=1; // Debug Modus!
			f=NULL;
		}
	}
	
	// open file for debug output
	if(debug==0) f=fopen("c:\\winnt\\capi20proxy.log","a+t");

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
				"Remote CAPI 2.0 Server",  // service name to display 
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
				if((threadhandle=CreateThread(NULL,0,ReceiveRequest,NULL,0,&thid))==NULL){
					DebugOut("ServiceMain(): CreateThread");
				} else {
					CloseHandle(threadhandle);
					recv_threads++;
				}
			}
		}
	}
	
	//Exiting
	DebugOut("Exiting ServiceMain()",FALSE);
   return; 
} 
 
DWORD WINAPI ReceiveRequest(LPVOID param){
	if(debug) DebugOut("ReceiverRequest(): Receiving Request",FALSE);
	fd_set test;
	test.fd_count=1;
	test.fd_array[0]=socke;
	if(select(0,&test,NULL,NULL,NULL)==SOCKET_ERROR){
		DebugOut("ReceiveRequest(): select",FALSE);
		recv_threads--;
		return 1;
	} else {
		if(test.fd_count==0){
			recv_threads--;
			return 1;
		}
	}
	
	SOCKADDR_IN from;
	char buffer[512];
	int len=sizeof(SOCKADDR_IN);

	if(recvfrom(socke,buffer,512,0,(SOCKADDR*)&from,&len)==SOCKET_ERROR){
		DWORD err=WSAGetLastError();
		SetLastError(err);
		DebugOut("ReceiveRequest(): recvfrom");
		recv_threads--;
		return err;
	}
	if(debug) DebugOut("ReceiveRequest(): Request received",FALSE);

	REQUEST *request=(REQUEST*) buffer;
	ANSWER answer;
		
	answer.type=request->type;

	char *buffer1=NULL,*buffer2=NULL;
	SOCKET connection;
	UINT len1=0,len2=0;
	DWORD dword,err;

	unsigned char cmd1,cmd2;

	err=0;

	switch(request->type)
	{
	case TYPE_REGISTER:
		if(debug) DebugOut("ReceiveRequest(): TYPE_REGISTER",FALSE);
		answer.data.ad_register.ret=CAPI_REGISTER(
			request->data.rd_register.MessageBufferSize,
			request->data.rd_register.maxLogicalConnection,
			request->data.rd_register.maxBDataBlocks,
			request->data.rd_register.maxBDataLen,
			&(answer.data.ad_register.ApplID));
		err=0;
		break;

	case TYPE_RELEASE:
		if(debug) DebugOut("ReceiveRequest(): TYPE_RELEASE",FALSE);
		answer.data.ad_release.ret=CAPI_RELEASE(request->data.rd_release.ApplID);
		err=0;
		break;

	case TYPE_PUT_MESSAGE:
		if(debug) DebugOut("ReceiveRequest(): TYPE_PUT_MESSAGE",FALSE);
		connection=CreateSocket(SOCK_STREAM,IPPROTO_TCP,0);
		if(socke==INVALID_SOCKET){
			err=GetLastError();
			SetLastError(err);
			DebugOut("ReceiveRequest()/TYPE_PUT_MESSAGE: CreateSocket");
			break;
		}
		
		err=connect(connection,(SOCKADDR*) &(request->data.rd_put_message.me),sizeof(SOCKADDR_IN));
		if(err==SOCKET_ERROR){
			err=WSAGetLastError();
			SetLastError(err);
			DebugOut("ReceiveRequest()/TYPE_PUT_MESSAGE: connect");
			answer.data.ad_put_message.ret=0x1108;
			closesocket(connection);
			break;
		}
		
		err=recv(connection,(char*)&len1,2,0);
		if((err==SOCKET_ERROR) || (err<2)){
			err=WSAGetLastError();
			SetLastError(err);
			DebugOut("ReceiveRequest()/TYPE_PUT_MESSAGE: recv");
			answer.data.ad_put_message.ret=0x1108;
			closesocket(connection);
			break;
		}
		
		buffer1=(char*)GlobalAlloc(GPTR,len1);
		if(buffer1==NULL){
			err=GetLastError();
			SetLastError(err);
			DebugOut("ReceiveRequest()/TYPE_PUT_MESSAGE: GlobalAlloc");
			answer.data.ad_put_message.ret=0x1108;
			closesocket(connection);
			break;
		}

		memcpy(buffer1,&len1,2);
		err=recv(connection,buffer1+2,len1-2,0);
		if((err==SOCKET_ERROR) || (err==0) || (err<len1-2)){
			err=WSAGetLastError();
			SetLastError(err);
			DebugOut("ReceiveRequest()/TYPE_PUT_MESSAGE: recv");
			answer.data.ad_put_message.ret=0x1108;
			closesocket(connection);
			break;
		}

		cmd1=buffer1[4];
		cmd2=buffer1[5];
		
		if((cmd1==0x86) && (cmd2==0x80)){
			if(len1<18){
				DebugOut("ReceiveRequest()/TYPE_PUT_MESSAGE2: Buffer too small",FALSE);
				answer.data.ad_put_message.ret=0x1108;
				break;
			}
			
			memcpy(&len2,buffer1+16,2);
			buffer2=(char*)GlobalAlloc(GPTR,len2);
			if(buffer2==NULL){
				err=GetLastError();
				SetLastError(err);
				DebugOut("ReceiveRequest()/TYPE_PUT_MESSAGE2: GlobalAlloc");
				answer.data.ad_put_message.ret=0x1108;
				closesocket(connection);
				break;
			}

			err=recv(connection,buffer2,len2,0);
			if((err==SOCKET_ERROR) || (err==0) || (err<len2)){
				err=WSAGetLastError();
				SetLastError(err);
				DebugOut("ReceiveRequest()/TYPE_PUT_MESSAGE2: recv");
				answer.data.ad_put_message.ret=0x1108;
				closesocket(connection);
				break;
			}

			dword=(DWORD)buffer2;
			memcpy(buffer1+12,&dword,4);
		}

		err=closesocket(connection);
		if(err==SOCKET_ERROR){
			err=WSAGetLastError();
			SetLastError(err);
			DebugOut("ReceiveRequest()/TYPE_PUT_MESSAGE: closesocket");
			answer.data.ad_put_message.ret=0x1108;
			break;
		}

		answer.data.ad_put_message.ret=CAPI_PUT_MESSAGE(request->data.rd_put_message.ApplID,buffer1);
		GlobalFree((LPVOID)buffer1);
		if(buffer2!=NULL) GlobalFree((LPVOID)buffer2);
		err=0;
		break;

	case TYPE_GET_MESSAGE:
		if(debug) DebugOut("ReceiveRequest(): TYPE_GET_MESSAGE",FALSE);
		connection=CreateSocket(SOCK_STREAM,IPPROTO_TCP,0);
		if(socke==INVALID_SOCKET){
			err=GetLastError();
			SetLastError(err);
			DebugOut("ReceiveRequest()/TYPE_GET_MESSAGE: CreateSocket");
			answer.data.ad_get_message.ret=0x1108;
			break;
		}
		
		err=connect(connection,(SOCKADDR*) &(request->data.rd_get_message.me),sizeof(SOCKADDR_IN));
		if(err==SOCKET_ERROR){
			err=WSAGetLastError();
			SetLastError(err);
			DebugOut("ReceiveRequest()/TYPE_GET_MESSAGE: connect");
			answer.data.ad_get_message.ret=0x1108;
			closesocket(connection);
			break;
		}
		
		answer.data.ad_get_message.ret=CAPI_GET_MESSAGE(
			request->data.rd_get_message.ApplID,
			(void**)&buffer1);

		if((answer.data.ad_get_message.ret & 0x1100)==NO_ERROR){
			memcpy(&len1,buffer1,2);
			
			err=send(connection,buffer1,2,0);
			if((err==SOCKET_ERROR) || (err==0)){
				err=WSAGetLastError();
				SetLastError(err);
				DebugOut("ReceiveRequest()/TYPE_GET_MESSAGE: send");
				answer.data.ad_get_message.ret=0x1108;
				closesocket(connection);
				break;
			}
			
			err=send(connection,buffer1+2,len1-2,0);
			if((err==SOCKET_ERROR) || (err==0)){
				err=WSAGetLastError();
				SetLastError(err);
				DebugOut("ReceiveRequest()/TYPE_GET_MESSAGE: send");
				answer.data.ad_get_message.ret=0x1108;
				closesocket(connection);
				break;
			}

			cmd1=buffer1[4];
			cmd2=buffer1[5];
			
			if((cmd1==0x86) && (cmd2==0x82)){
				
				memcpy(&len2,buffer1+16,2);
				DWORD pointer;
				memcpy(&pointer,buffer1+12,4);
				buffer2=(char*)(LPVOID)pointer;
				if(buffer2==NULL){
					DebugOut("ReceiveRequest()/TYPE_GET_MESSAGE2: NULL-Pointer in message detected",FALSE);
					answer.data.ad_get_message.ret=0x1108;
					closesocket(connection);
					break;
				}

				err=send(connection,buffer2,len2,0);
				if((err==SOCKET_ERROR) || (err==0)){
					err=WSAGetLastError();
					SetLastError(err);
					DebugOut("ReceiveRequest()/TYPE_GET_MESSAGE2: send");
					answer.data.ad_get_message.ret=0x1108;
					closesocket(connection);
					break;
				}
			}
		}
		
		err=closesocket(connection);
		if(err==SOCKET_ERROR){
			err=WSAGetLastError();
			SetLastError(err);
			DebugOut("ReceiveRequest()/TYPE_GET_MESSAGE2: closesocket");
			answer.data.ad_get_message.ret=0x1108;
			break;
		}

		err=0;
		break;

	case TYPE_WAIT_FOR_SIGNAL:
		if(debug) DebugOut("ReceiveRequest(): TYPE_WAIT_FOR_SIGNAL",FALSE);
		answer.data.ad_wait_for_signal.ret=CAPI_WAIT_FOR_SIGNAL(request->data.rd_wait_for_signal.ApplID);
		err=0;
		break;

	case TYPE_GET_MANUFACTURER:
		if(debug) DebugOut("ReceiveRequest(): TYPE_GET_MANUFACTURER",FALSE);
		answer.data.ad_get_manufacturer.ret=CAPI_GET_MANUFACTURER(answer.data.ad_get_manufacturer.szBuffer);
		err=0;
		break;

	case TYPE_GET_VERSION:
		if(debug) DebugOut("ReceiveRequest(): TYPE_GET_VERSION",FALSE);
		answer.data.ad_get_version.ret=CAPI_GET_VERSION(
			&(answer.data.ad_get_version.CAPIMajor),
			&(answer.data.ad_get_version.CAPIMinor),
			&(answer.data.ad_get_version.ManufacturerMajor),
			&(answer.data.ad_get_version.MAnufacturerMinor));
		err=0;
		break;

	case TYPE_GET_SERIAL_NUMBER:
		if(debug) DebugOut("ReceiveRequest(): TYPE_GET_SERIAL_NUMBER",FALSE);
		answer.data.ad_get_serial_number.ret=CAPI_GET_SERIAL_NUMBER(answer.data.ad_get_serial_number.szBuffer);
		err=0;
		break;

	case TYPE_GET_PROFILE:
		if(debug) DebugOut("ReceiveRequest(): TYPE_GET_PROFILE",FALSE);
		answer.data.ad_get_profile.ret=CAPI_GET_PROFILE(
			answer.data.ad_get_profile.szBuffer,
			request->data.rd_get_profile.CtrlNr);
		err=0;
		break;

	case TYPE_INSTALLED:
		if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_INSTALLED",FALSE);
		answer.data.ad_installed.ret=CAPI_INSTALLED();
		err=0;
		break;

	case TYPE_WHO_AM_I:
		if(debug) DebugOut("ReceiveRequest(): TYPE_WHO_AM_I",FALSE);
		answer.data.ad_who_am_i.you=from;
		err=0;
		break;

	default:
		DebugOut("ReceiveRequest(): Invalid Message Type",FALSE);
		err=1;
		recv_threads--;
		return 1;
	}

	if((sendto(socke,(LPCTSTR)&answer,sizeof(answer),0,(SOCKADDR*)&from,sizeof(SOCKADDR_IN)))
		==SOCKET_ERROR){
		DWORD err=WSAGetLastError();
		SetLastError(err);
		DebugOut("ReceiveRequest(): sendto");
		recv_threads--;
		return err;
	}

	recv_threads--;
	return NO_ERROR;
}

// Stub initialization function. 
DWORD ServiceInitialization(DWORD argc, LPTSTR *argv,DWORD *specificError) 
{
	socke=CreateSocket(SOCK_DGRAM,IPPROTO_UDP,7103);
	if(socke==INVALID_SOCKET){
		DWORD err=GetLastError();
		SetLastError(err);
		DebugOut("InitService(): CreateSocket");
		return err;
	}

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

