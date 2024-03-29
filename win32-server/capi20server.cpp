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
 * Revision 1.13  2002/05/12 07:06:10  butzist
 * updated
 *
 * Revision 1.12  2002/04/11 09:01:23  butzist
 * only some minor bugs
 *
 * Revision 1.11  2002/04/09 16:28:09  butzist
 * show the console again before quitting (so the user can see the errors and close it)
 *
 * Revision 1.10  2002/04/09 14:30:35  butzist
 * works now quite fine
 * added "tray-mode" for Win9x (when started without parameters)
 * parameter "-service" for logging into file and starting ServiceControlDispatcher
 *
 * Revision 1.9  2002/04/08 20:46:53  butzist
 * voice communication performance improved
 * doing automatic CAPI_RELEASE for each registered application when connection to client breaks
 * seems not to hang
 *
 * Revision 1.8  2002/03/29 07:52:06  butzist
 * seems to work (got problems with DATA_B3)
 *
 * Revision 1.7  2002/03/22 16:48:06  butzist
 * just coded a little bit bt didn't finish
 *
 * Revision 1.6  2002/03/21 15:16:42  butzist
 * started rewriting for new protocol
 *
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
#include "resource.h"
#include "mytypes.h"

#define _TIMEOUT		-1		// no timeout

#define	_VERSION_MAJOR	1
#define _VERSION_MINOR	3

const __version_t	_SERVER_VERSION = { _VERSION_MAJOR, _VERSION_MINOR };
const char* _SERVER_NAME = "CAPI 2.0 Proxy Win32 Server";

u32 AUTH_TYPES = AUTH_NO_AUTH; //AUTH_BY_IP | AUTH_USERPASS
u32	AUTH_REQUIRED = false;

char	LOGFILE[256]=__LOGFILE;
DWORD	PORT=__PORT;

SERVICE_STATUS          ServiceStatus; 
SERVICE_STATUS_HANDLE   ServiceStatusHandle; 
int run,pause,debug,service; 
FILE *f;
HANDLE RunningThread;
DWORD session=0;
SOCKET sockets[_MAX_SESSIONS];
SOCKET socke;
IN_ADDR localaddr = {INADDR_ANY};

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
	f=NULL;
	RunningThread=0;
	debug=0;
	service=0;
	run=1;
	pause=0;

	int i;

	for(i=1;i<argc;i++) {
		if(strcmp(argv[i],"-debug")==0){
			debug=1; // Debug Modus!
		}
	}

	for(i=1;i<argc;i++) {
		if(strcmp(argv[i],"-service")==0){
			service=1; // Service Modus!
		}
	}

	for(i=0; i<_MAX_SESSIONS; i++) // init sockets for the sessions
	{
		sockets[i]=INVALID_SOCKET;
	}

	
	// open file for debug output
	if(service) f=fopen(LOGFILE,"a+t");

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
			strcat(lpszBinaryPathName," -service");


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
			printf("\r\nUse folloging switches:\r\n\t-i\tInstall as Service under Windows NT\r\n\t-u\tUninstall the Service\r\n\t-debug\tRun Debug mode\r\n\t-service\tRun as a Service\r\n");
			return 0;
		}
	}


	//Startup Windows Socket Architecture
	
	WSADATA wsadata;
	if(WSAStartup(MAKEWORD(2,2),&wsadata)!=0){
		DebugOut("main(): WSAStartup");
		return 1;
	}
	
	//Get some values from the registry
	char ipstr[256];
	DWORD err,len;
	HKEY key;

	err=RegOpenKeyEx(HKEY_LOCAL_MACHINE,"Software\\capi20proxy\\server",0,KEY_QUERY_VALUE,&key);
	if(err!=ERROR_SUCCESS){
		::MessageBox(NULL,"Could not open Registry Key \"HKEY_LOCAL_MACHINE\\Software\\capi20proxy\\server\"","Error",MB_OK);
		return FALSE;
	}
	
	len=256;
	err=RegQueryValueEx(key,"logfile",NULL,NULL,(unsigned char*)LOGFILE,&len);
	if(err!=ERROR_SUCCESS){
		::MessageBox(NULL,"Could not read Regitry Value \"HKEY_LOCAL_MACHINE\\Software\\capi20proxy\\server\\logfile\"","Error",MB_OK);
		return FALSE;
	}

	len=256;
	err=RegQueryValueEx(key,"bind",NULL,NULL,(unsigned char*)ipstr,&len);
	if(err==ERROR_SUCCESS){
		localaddr.S_un.S_addr=inet_addr(ipstr);
	}

	len=4;
	err=RegQueryValueEx(key,"port",NULL,NULL,(unsigned char*)&PORT,&len);
	if(err!=ERROR_SUCCESS){
		PORT=__PORT;
	}

	len=4;
	err=RegQueryValueEx(key,"auth_required",NULL,NULL,(unsigned char*)&AUTH_REQUIRED,&len);
	if(err!=ERROR_SUCCESS){
		AUTH_REQUIRED=false;
	}

	len=4;
	err=RegQueryValueEx(key,"auth_types",NULL,NULL,(unsigned char*)&AUTH_TYPES,&len);
	if(err!=ERROR_SUCCESS){
		AUTH_TYPES=AUTH_NO_AUTH;
	}

	RegCloseKey(key);

	
	// The Debug mode starts the server in a console (not as a service in 
	// the background). All messages are sent to the console.
	if(service){
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
			HWND console=NULL;

			if(!debug)
			{
				char title[512];		// ein bissl umständlich... aber GetConsoleWindow() gibt es erst ab Win2k
				GetConsoleTitle(title,512);
				console=FindWindow(NULL,title);
				ShowWindow(console,SW_HIDE);
			}
			
			DialogBox(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_DIALOG),NULL,dialogProc);
			// this is a modal dialog box
			
			if(console!=NULL)
			{
				ShowWindow(console,SW_SHOW);
			}

			ServiceExit();
		}
	}

	DebugOut("Exiting main()",FALSE);
	WSACleanup();
	_fcloseall();
	return 0;
}

INT_PTR CALLBACK dialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	NOTIFYICONDATA ndata;

	
	ndata.cbSize=sizeof(NOTIFYICONDATA);
	ndata.hWnd=hwnd;
	ndata.uID=0;
	ndata.uCallbackMessage=WM_USER;
	ndata.hIcon=LoadIcon(GetModuleHandle(NULL),MAKEINTRESOURCE(IDI_ICON));
	strcpy(ndata.szTip,"CAPI 2.0 Proxy Win32 Server");
	ndata.uFlags=NIF_MESSAGE | NIF_TIP | NIF_ICON;
	
	switch(msg)
	{
	case WM_INITDIALOG:
		Shell_NotifyIcon(NIM_ADD,&ndata);
		return FALSE;

	case WM_COMMAND:

		switch((UINT)wParam)
		{
		case IDC_PAUSE:
			if(pause==0)
				pause=1;
			else
				pause=0;
			return TRUE;

		case IDOK:
			run=0;
			Shell_NotifyIcon(NIM_DELETE,&ndata);
			EndDialog(hwnd, IDOK);
			return TRUE;

		default:
			return FALSE;
		}

	case WM_USER:
		if(((UINT)lParam)==WM_LBUTTONDOWN || ((UINT)lParam)==WM_RBUTTONDOWN)
		{
			ShowWindow(hwnd,SW_SHOW);
		}
		return TRUE;

	case WM_CLOSE:
		ShowWindow(hwnd,SW_HIDE);
		return TRUE;

	default:
		return FALSE;
	}
}


DWORD WINAPI TestStart(LPVOID param){
	ServiceStart(0,NULL);
	return 0;
}

void WINAPI ServiceStart (DWORD argc, LPTSTR *argv) 
{ 
	DWORD status; 
	DWORD specificError; 

    if(service){
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

	if(service){
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
	    if(service){
			ServiceStatus.dwCurrentState       = SERVICE_STOPPED; 
			ServiceStatus.dwCheckPoint         = 0; 
			ServiceStatus.dwWaitHint           = 0; 
			ServiceStatus.dwWin32ExitCode      = status; 
			ServiceStatus.dwServiceSpecificExitCode = specificError; 
 
			SetServiceStatus (ServiceStatusHandle, &ServiceStatus); 
		}
        return; 
    } 
 
    
	if(service){
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
			FD_ZERO(&test);
			FD_SET(socke,&test);
			
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

				int sess=allocSession(connection);
				if(sess==-1)
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

int allocSession(SOCKET socke)
{
	for(int i=0; i<_MAX_SESSIONS; i++)
	{
		if(sockets[i]==INVALID_SOCKET)
		{
			sockets[i]=socke;
			return i;
		}
	}
	return 0;
}

void freeSession(int sess)
{
	if(sess>=_MAX_SESSIONS) return;

	sockets[sess]=INVALID_SOCKET;
}

void add_app(DWORD _ApplID, char* _list)
{
  _list[_ApplID]=1;
}

void del_app(DWORD _ApplID, char* _list)
{
  _list[_ApplID]=0;
}

DWORD getfirst_app(char* _list)
{
  int i=1;
  while(i<=CAPI_MAXAPPL) {
    if(_list[i]==1) {
      _list[i]=0;
      return i;
    }
  }
  return 0;
}


DWORD exec_capi_register(REQUEST_HEADER* rheader,REQUEST_CAPI_REGISTER* rbody,char* rdata, client_data* client)
{
	char answer[_MSG_SIZE];
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_REGISTER *abody=(ANSWER_CAPI_REGISTER*)(answer+sizeof(ANSWER_HEADER));
	char* adata=answer+sizeof(ANSWER_HEADER)+sizeof(ANSWER_CAPI_REGISTER);

	aheader->header_len=sizeof(ANSWER_HEADER);
	aheader->body_len=sizeof(ANSWER_CAPI_REGISTER);
	aheader->data_len=0;
	aheader->message_type=TYPE_CAPI_REGISTER;
	aheader->message_id=rheader->message_id;
	aheader->session_id=client->session;
	aheader->proxy_error=PERROR_NONE;

	// the applId is a unsigned long!!!!! Fitzle, du hast es wieder versaut	
	aheader->capi_error=CAPI_REGISTER(rbody->messageBufferSize,
		rbody->maxLogicalConnection,
		rbody->maxBDataBlocks,
		rbody->maxBDataLen,
		&(aheader->app_id));

	if(aheader->capi_error==0x0000)
	{
		// add registered app to list
		add_app(aheader->app_id, client->registered_apps);
	}

	aheader->message_len=aheader->header_len+aheader->body_len+aheader->data_len;
	return send(client->socket,answer,aheader->message_len,0);
}

DWORD exec_capi_release(REQUEST_HEADER* rheader,REQUEST_CAPI_RELEASE* rbody,char* rdata, client_data* client)
{
	char answer[_MSG_SIZE];
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_RELEASE *abody=(ANSWER_CAPI_RELEASE*)(answer+sizeof(ANSWER_HEADER));
	char* adata=answer+sizeof(ANSWER_HEADER)+sizeof(ANSWER_CAPI_RELEASE);

	aheader->header_len=sizeof(ANSWER_HEADER);
	aheader->body_len=sizeof(ANSWER_CAPI_RELEASE);
	aheader->data_len=0;
	aheader->message_type=TYPE_CAPI_RELEASE;
	aheader->message_id=rheader->message_id;
	aheader->session_id=client->session;
	aheader->proxy_error=PERROR_NONE;

	aheader->capi_error=CAPI_RELEASE(rheader->app_id);

	if(aheader->capi_error==0x0000)
	{
		// delete registered app from list
		del_app(rheader->app_id, client->registered_apps);
	}

	aheader->message_len=aheader->header_len+aheader->body_len+aheader->data_len;
	return send(client->socket,answer,aheader->message_len,0);
}

DWORD exec_capi_putmessage(REQUEST_HEADER* rheader,REQUEST_CAPI_PUTMESSAGE* rbody,char* rdata, client_data* client)
{
	char answer[_MSG_SIZE];
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_PUTMESSAGE *abody=(ANSWER_CAPI_PUTMESSAGE*)(answer+sizeof(ANSWER_HEADER));
	char* adata=answer+sizeof(ANSWER_HEADER)+sizeof(ANSWER_CAPI_PUTMESSAGE);

	aheader->header_len=sizeof(ANSWER_HEADER);
	aheader->body_len=sizeof(ANSWER_CAPI_PUTMESSAGE);
	aheader->data_len=0;
	aheader->message_type=TYPE_CAPI_PUTMESSAGE;
	aheader->message_id=rheader->message_id;
	aheader->session_id=client->session;
	aheader->proxy_error=PERROR_NONE;

	if(rheader->data_len<8) // data too short
	{
		aheader->proxy_error=PERROR_PARSE_ERROR;
	} else {
		UINT len1=0;
		memcpy(&len1,rdata,2);		// get the length of the CAPI message

		if(rheader->data_len<len1) // data too short
		{
			aheader->proxy_error=PERROR_PARSE_ERROR;
		} else {
			unsigned char cmd1=CAPI_CMD1(rdata);
			unsigned char cmd2=CAPI_CMD2(rdata);

			if((cmd1==0x86) && (cmd2==0x80))	// If we got a DATA_B3 REQ
			{
				if(len1<18)
				{
					aheader->proxy_error=PERROR_PARSE_ERROR;
				} else {
                    UINT len2=0;
					memcpy(&len2,rdata+16,2);	// get the length of the data
					
					if(len1+len2>rheader->data_len)
					{
						aheader->proxy_error=PERROR_PARSE_ERROR;
					} else {
						DWORD pointer=(DWORD)rdata+len1;		// data is appended to the message
						memcpy(rdata+12,&pointer,4);
						
						// call the CAPI function
						aheader->capi_error=CAPI_PUT_MESSAGE(rheader->app_id,rdata);
					}
				}
			} else {
				// call the CAPI function
				aheader->capi_error=CAPI_PUT_MESSAGE(rheader->app_id,rdata);
			}
		}
	}

	aheader->message_len=aheader->header_len+aheader->body_len+aheader->data_len;
	return send(client->socket,answer,aheader->message_len,0);
}

DWORD exec_capi_getmessage(REQUEST_HEADER* rheader,REQUEST_CAPI_GETMESSAGE* rbody,char* rdata, client_data* client)
{
	char answer[_MSG_SIZE];
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_GETMESSAGE *abody=(ANSWER_CAPI_GETMESSAGE*)(answer+sizeof(ANSWER_HEADER));
	char* adata=answer+sizeof(ANSWER_HEADER)+sizeof(ANSWER_CAPI_GETMESSAGE);

	aheader->header_len=sizeof(ANSWER_HEADER);
	aheader->body_len=sizeof(ANSWER_CAPI_GETMESSAGE);
	aheader->data_len=0;
	aheader->message_type=TYPE_CAPI_GETMESSAGE;
	aheader->message_id=rheader->message_id;
	aheader->session_id=client->session;
	aheader->proxy_error=PERROR_NONE;

	char* message;

	if((aheader->capi_error=CAPI_GET_MESSAGE(rheader->app_id,(LPVOID*) &message))==0x0000)
	{
		UINT len1=0;
		memcpy(&len1,message,2);		// get the length of the CAPI message

		memcpy(adata,message,len1);		// copy the message to the answer
		aheader->data_len=len1;

		unsigned char cmd1=CAPI_CMD1(adata);
		unsigned char cmd2=CAPI_CMD2(adata);

		if((cmd1==0x86) && (cmd2==0x82))	// If we got a DATA_B3 IND
		{
			if(len1<18)
			{
				aheader->proxy_error=PERROR_PARSE_ERROR;
			} else {
                UINT len2=0;
				memcpy(&len2,adata+16,2);	// get the length of the data
				
				DWORD pointer=0;
				memcpy(&pointer,adata+12,4);	// get the start address of the data
				
				memcpy(adata+len1,(LPVOID)pointer,len2);	// append the data to the CAPI message

				aheader->data_len+=len2;
			}
		}
	}

	aheader->message_len=aheader->header_len+aheader->body_len+aheader->data_len;
	return send(client->socket,answer,aheader->message_len,0);
}

DWORD WINAPI waiter(LPVOID param)
{
	waiter_data* data=(waiter_data*)param;
	SOCKET socke=data->socket;
	char answer[_MSG_SIZE];
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_WAITFORSIGNAL *abody=(ANSWER_CAPI_WAITFORSIGNAL*)(answer+sizeof(ANSWER_HEADER));
	char* adata=answer+sizeof(ANSWER_HEADER)+sizeof(ANSWER_CAPI_WAITFORSIGNAL);

	aheader->header_len=sizeof(ANSWER_HEADER);
	aheader->body_len=sizeof(ANSWER_CAPI_WAITFORSIGNAL);
	aheader->data_len=0;
	aheader->message_type=TYPE_CAPI_WAITFORSIGNAL;
	aheader->message_id=data->message_id;
	aheader->session_id=data->session_id;
	aheader->proxy_error=PERROR_NONE;

	aheader->capi_error=CAPI_WAIT_FOR_SIGNAL(data->ApplID);	// call the CAPI function

	myfree(param);

	aheader->message_len=aheader->header_len+aheader->body_len+aheader->data_len;
	return send(socke,answer,aheader->message_len,0);
}

DWORD exec_capi_waitforsignal(REQUEST_HEADER* rheader,REQUEST_CAPI_WAITFORSIGNAL* rbody,char* rdata, client_data* client)
{
	waiter_data* data = (waiter_data*) mymalloc(sizeof(waiter_data));

	data->socket=client->socket;
	data->ApplID=rheader->app_id;
	data->message_id=rheader->message_id;
	data->session_id=client->session;
	
	DWORD thid;
	if(CreateThread(NULL,1024,waiter,(LPVOID)data,0,&thid)==NULL)
	{
		char answer[_MSG_SIZE];
		ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
		ANSWER_CAPI_WAITFORSIGNAL*abody=(ANSWER_CAPI_WAITFORSIGNAL*)(answer+sizeof(ANSWER_HEADER));
		char* adata=answer+sizeof(ANSWER_HEADER)+sizeof(ANSWER_CAPI_WAITFORSIGNAL);

		aheader->header_len=sizeof(ANSWER_HEADER);
		aheader->body_len=sizeof(ANSWER_CAPI_WAITFORSIGNAL);
		aheader->data_len=0;
		aheader->message_type=TYPE_CAPI_WAITFORSIGNAL;
		aheader->message_id=rheader->message_id;
		aheader->session_id=client->session;
		aheader->proxy_error=PERROR_NO_RESSOURCES;

		aheader->message_len=aheader->header_len+aheader->body_len+aheader->data_len;
		send(client->socket,answer,aheader->message_len,0);

		myfree((LPVOID)data);
	}
	return NO_ERROR;
}


DWORD exec_capi_manufacturer(REQUEST_HEADER* rheader,REQUEST_CAPI_MANUFACTURER* rbody,char* rdata, client_data* client)
{
	char answer[_MSG_SIZE];
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_MANUFACTURER *abody=(ANSWER_CAPI_MANUFACTURER*)(answer+sizeof(ANSWER_HEADER));
	char* adata=answer+sizeof(ANSWER_HEADER)+sizeof(ANSWER_CAPI_MANUFACTURER);

	aheader->header_len=sizeof(ANSWER_HEADER);
	aheader->body_len=sizeof(ANSWER_CAPI_MANUFACTURER);
	aheader->data_len=0;
	aheader->message_type=TYPE_CAPI_MANUFACTURER;
	aheader->message_id=rheader->message_id;
	aheader->session_id=client->session;
	aheader->proxy_error=PERROR_NONE;

	aheader->capi_error=CAPI_GET_MANUFACTURER(abody->manufacturer);		// call me Mr. CAPI, call me .... :-)

	aheader->message_len=aheader->header_len+aheader->body_len+aheader->data_len;
	return send(client->socket,answer,aheader->message_len,0);
}

DWORD exec_capi_version(REQUEST_HEADER* rheader,REQUEST_CAPI_VERSION* rbody,char* rdata, client_data* client)
{
	char answer[_MSG_SIZE];
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_VERSION *abody=(ANSWER_CAPI_VERSION*)(answer+sizeof(ANSWER_HEADER));
	char* adata=answer+sizeof(ANSWER_HEADER)+sizeof(ANSWER_CAPI_VERSION);

	aheader->header_len=sizeof(ANSWER_HEADER);
	aheader->body_len=sizeof(ANSWER_CAPI_VERSION);
	aheader->data_len=0;
	aheader->message_type=TYPE_CAPI_VERSION;
	aheader->message_id=rheader->message_id;
	aheader->session_id=client->session;
	aheader->proxy_error=PERROR_NONE;

	DWORD versions[4];

	aheader->capi_error=CAPI_GET_VERSION(&versions[0],&versions[1],&versions[2],&versions[3]);
	abody->driver.major=versions[0];
	abody->driver.minor=versions[1];
	abody->manufacturer.major=versions[2];
	abody->manufacturer.minor=versions[3];

	aheader->message_len=aheader->header_len+aheader->body_len+aheader->data_len;
	return send(client->socket,answer,aheader->message_len,0);
}

DWORD exec_capi_serial(REQUEST_HEADER* rheader,REQUEST_CAPI_SERIAL* rbody,char* rdata, client_data* client)
{
	char answer[_MSG_SIZE];
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_SERIAL *abody=(ANSWER_CAPI_SERIAL*)(answer+sizeof(ANSWER_HEADER));
	char* adata=answer+sizeof(ANSWER_HEADER)+sizeof(ANSWER_CAPI_SERIAL);

	aheader->header_len=sizeof(ANSWER_HEADER);
	aheader->body_len=sizeof(ANSWER_CAPI_SERIAL);
	aheader->data_len=0;
	aheader->message_type=TYPE_CAPI_SERIAL;
	aheader->message_id=rheader->message_id;
	aheader->session_id=client->session;
	aheader->proxy_error=PERROR_NONE;

	aheader->capi_error=CAPI_GET_SERIAL_NUMBER(abody->serial);		// calling the CAPI

	aheader->message_len=aheader->header_len+aheader->body_len+aheader->data_len;
	return send(client->socket,answer,aheader->message_len,0);
}

DWORD exec_capi_profile(REQUEST_HEADER* rheader,REQUEST_CAPI_PROFILE* rbody,char* rdata, client_data* client)
{
	char answer[_MSG_SIZE];
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_PROFILE *abody=(ANSWER_CAPI_PROFILE*)(answer+sizeof(ANSWER_HEADER));
	char* adata=answer+sizeof(ANSWER_HEADER)+sizeof(ANSWER_CAPI_PROFILE);

	aheader->header_len=sizeof(ANSWER_HEADER);
	aheader->body_len=sizeof(ANSWER_CAPI_PROFILE);
	aheader->data_len=0;
	aheader->message_type=TYPE_CAPI_PROFILE;
	aheader->message_id=rheader->message_id;
	aheader->session_id=client->session;
	aheader->proxy_error=PERROR_NONE;

	aheader->capi_error=CAPI_GET_PROFILE(abody->profile,rheader->controller_id);

	aheader->message_len=aheader->header_len+aheader->body_len+aheader->data_len;
	return send(client->socket,answer,aheader->message_len,0);
}

DWORD exec_capi_installed(REQUEST_HEADER* rheader,REQUEST_CAPI_INSTALLED* rbody,char* rdata, client_data* client)
{
	char answer[_MSG_SIZE];
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_CAPI_INSTALLED *abody=(ANSWER_CAPI_INSTALLED*)(answer+sizeof(ANSWER_HEADER));
	char* adata=answer+sizeof(ANSWER_HEADER)+sizeof(ANSWER_CAPI_INSTALLED);

	aheader->header_len=sizeof(ANSWER_HEADER);
	aheader->body_len=sizeof(ANSWER_CAPI_INSTALLED);
	aheader->data_len=0;
	aheader->message_type=TYPE_CAPI_INSTALLED;
	aheader->message_id=rheader->message_id;
	aheader->session_id=client->session;
	aheader->proxy_error=PERROR_NONE;

	aheader->capi_error=CAPI_INSTALLED();

	aheader->message_len=aheader->header_len+aheader->body_len+aheader->data_len;
	return send(client->socket,answer,aheader->message_len,0);
}

DWORD exec_proxy_helo(REQUEST_HEADER* rheader, REQUEST_PROXY_HELO* rbody, char* rdata, client_data* client)
{
	char answer[_MSG_SIZE];
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_PROXY_HELO *abody=(ANSWER_PROXY_HELO*)(answer+sizeof(ANSWER_HEADER));
	char* adata=answer+sizeof(ANSWER_HEADER)+sizeof(ANSWER_PROXY_HELO);

	aheader->header_len=sizeof(ANSWER_HEADER);
	aheader->body_len=sizeof(ANSWER_PROXY_HELO);
	aheader->data_len=0;
	aheader->message_type=TYPE_PROXY_HELO;
	aheader->message_id=rheader->message_id;
	aheader->session_id=0;
	aheader->proxy_error=PERROR_NONE;

	memcpy(client->name,rbody->name,64);
	client->name[63]=0;

	client->version=rbody->version;

	aheader->app_id=0;
	abody->auth_type=AUTH_TYPES;
	strcpy(abody->name,_SERVER_NAME);
	abody->os=OS_TYPE_WINDOWS;
	abody->timeout=_TIMEOUT;
	abody->version=_SERVER_VERSION;

	if(client->version.major<_SERVER_VERSION.major)
	{
		aheader->proxy_error=PERROR_INCOMPATIBLE_VERSION;
	} else {
		client->os=rbody->os;
		aheader->session_id=client->session;
	}
	
	aheader->message_len=aheader->header_len+aheader->body_len+aheader->data_len;
	return send(client->socket,answer,aheader->message_len,0);
}

DWORD exec_proxy_keepalive(REQUEST_HEADER* rheader,REQUEST_PROXY_KEEPALIVE* rbody,char* rdata, client_data* client)
{
	// NYI
	return NO_ERROR;
}

int try_auth_byip(SOCKET s)
{
	sockaddr_in addr;
	char snet[16];
	char smask[16];
	DWORD net, mask;
	DWORD len,len2;
	
	len=sizeof(addr);
	
	if(getsockname(s,(sockaddr*)&addr,(int*)&len)!=0) {
		DebugOut("try_auth_byip: getsockname()",FALSE);
		return -1;
	}
	
	DWORD ip=addr.sin_addr.S_un.S_addr;
	DWORD err;
	HKEY key1,key2;

	err=RegOpenKeyEx(HKEY_LOCAL_MACHINE,"Software\\capi20proxy\\server\\hosts_allow",0,KEY_QUERY_VALUE,&key1);
	if(err!=ERROR_SUCCESS){
		DebugOut("try_auth_byip: Could not open Registry Key \"HKEY_LOCAL_MACHINE\\Software\\capi20proxy\\server\\hosts_allow\"",FALSE);
		return -1;
	}

	err=RegOpenKeyEx(HKEY_LOCAL_MACHINE,"Software\\capi20proxy\\server\\hosts_deny",0,KEY_QUERY_VALUE,&key2);
	if(err!=ERROR_SUCCESS){
		DebugOut("try_auth_byip: Could not open Registry Key \"HKEY_LOCAL_MACHINE\\Software\\capi20proxy\\server\\hosts_deny\"",FALSE);
		return -1;
	}

	len=len2=16;
	int i=0;
	int allowed=false;
	int denied=false;
	
	do {
		err=RegEnumValue(key1,i++,snet,&len,0,NULL,(unsigned char*)smask,&len2);
		if(err==ERROR_SUCCESS){
			net=inet_addr(snet);
			mask=inet_addr(smask);
			
			if((net&mask)==(ip&mask)) {
				allowed=true;
				break;
			}
		}
	} while(err==ERROR_SUCCESS);

	i=0;
	do {
		err=RegEnumValue(key2,i++,snet,&len,0,NULL,(unsigned char*)smask,&len2);
		if(err==ERROR_SUCCESS){
			net=inet_addr(snet);
			mask=inet_addr(smask);
			
			if((net&mask)==(ip&mask)) {
				denied=true;
				break;
			}
		}
	} while(err==ERROR_SUCCESS);
	
	RegCloseKey(key1);
	RegCloseKey(key2);
	
	if(allowed && (!denied)) {
		return 1;
	} else {
		return -1;
	}
}

int try_auth_userpass(char* user, char* pass)
{
	DWORD err;
	HKEY key;
	DWORD len;
	char test[256];
	int allowed=0;
	
	if(user==NULL)
		return -1;

	err=RegOpenKeyEx(HKEY_LOCAL_MACHINE,"Software\\capi20proxy\\server\\users",0,KEY_QUERY_VALUE,&key);
	if(err!=ERROR_SUCCESS){
		DebugOut("try_auth_userpass: Could not open Registry Key \"HKEY_LOCAL_MACHINE\\Software\\capi20proxy\\server\\users\"",FALSE);
		return -1;
	}
	
	len=256;
	err=RegQueryValueEx(key,user,NULL,NULL,(unsigned char*)test,&len);
	if(err!=ERROR_SUCCESS){
		return -1;		//user not found
	} else {
		if(strcmp(pass,test)==0) {
			allowed=1;
		}
	}

	RegCloseKey(key);
	if(allowed) {
		return 1;
	} else {
		return -1;
	}
}

DWORD exec_proxy_auth(REQUEST_HEADER* rheader,REQUEST_PROXY_AUTH* rbody,char* rdata, client_data* client)
{
	char answer[_MSG_SIZE];
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_PROXY_AUTH *abody=(ANSWER_PROXY_AUTH*)(answer+sizeof(ANSWER_HEADER));
	char* adata=answer+sizeof(ANSWER_HEADER)+sizeof(ANSWER_PROXY_AUTH);

	aheader->header_len=sizeof(ANSWER_HEADER);
	aheader->body_len=sizeof(ANSWER_PROXY_AUTH);
	aheader->data_len=0;
	aheader->message_type=TYPE_PROXY_AUTH;
	aheader->message_id=rheader->message_id;
	aheader->session_id=client->session;
	
	if(client->auth_data) {
		myfree(client->auth_data);
		client->auth_data=NULL;
	}
	client->auth_type=AUTH_NO_AUTH;

	switch(rbody->auth_type)
	{
	case AUTH_BY_IP:
		if(AUTH_TYPES&AUTH_BY_IP) {
			if(try_auth_byip(client->socket)!=-1) {
				client->auth_type=AUTH_BY_IP;
				aheader->proxy_error=PERROR_NONE;
			} else {
				aheader->proxy_error=PERROR_ACCESS_DENIED;
			}
		} else {
		aheader->proxy_error=PERROR_AUTH_TYPE_NOT_SUPPORTED;
		} 
		break;

	case AUTH_USERPASS:
		if(AUTH_TYPES&AUTH_USERPASS) {

			char user[256];
			char pass[256];
		
			if(rheader->data_len>1) {
				rdata[rheader->data_len]=0;	// data_len should be verified!
				int len1=strlen(rdata);
				int len2=rheader->data_len-len1;
				if(len1>255 || len2>255)
				{
					DebugOut("exex_proxy_auth: user/pass too long",FALSE);
					return -1;
				}
				
				strcpy(user,rdata);
				rdata+=strlen(user)+1;
				strcpy(pass,rdata);
			} else {
				user[0]=pass[0]=0;
			}

			if(try_auth_userpass(user,pass)!=-1) {
				client->auth_type=AUTH_USERPASS;
				aheader->proxy_error=PERROR_NONE;
			} else {
				aheader->proxy_error=PERROR_ACCESS_DENIED;
			}
		} else {
			aheader->proxy_error=PERROR_AUTH_TYPE_NOT_SUPPORTED;
		}
		break;

	case AUTH_NO_AUTH:
		if(!AUTH_REQUIRED) {
			aheader->proxy_error=PERROR_NONE;
		} else {
			aheader->proxy_error=PERROR_ACCESS_DENIED;
		}
		break;

	default:
		aheader->proxy_error=PERROR_AUTH_TYPE_NOT_SUPPORTED;
	}	

	aheader->message_len=aheader->header_len+aheader->body_len+aheader->data_len;
	return send(client->socket,answer,aheader->message_len,0);
}

DWORD exec_proxy_shutdown(client_data* client,const char* message)
{
	char answer[_MSG_SIZE];
	ANSWER_HEADER *aheader=(ANSWER_HEADER*)answer;
	ANSWER_PROXY_SHUTDOWN *abody=(ANSWER_PROXY_SHUTDOWN*)(answer+sizeof(ANSWER_HEADER));
	char* adata=answer+sizeof(ANSWER_HEADER)+sizeof(ANSWER_PROXY_SHUTDOWN);

	aheader->header_len=sizeof(ANSWER_HEADER);
	aheader->body_len=sizeof(ANSWER_PROXY_SHUTDOWN);
	aheader->data_len=0;
	aheader->message_type=TYPE_PROXY_SHUTDOWN;
	aheader->message_id=0;
	aheader->session_id=client->session;
	aheader->proxy_error=PERROR_NONE;

	strcpy(abody->reason,message);

	aheader->message_len=aheader->header_len+aheader->body_len+aheader->data_len;
	return send(client->socket,answer,aheader->message_len,0);
}

bool verifyRequest(char* msg)
{
	REQUEST_HEADER *header=(REQUEST_HEADER*)msg;
	UINT type=header->message_type;

	if(header->header_len<sizeof(REQUEST_HEADER))
	{
		// header too short
		return false;
	}

	if(header->body_len<(unsigned int)rbodysize(type))
	{
		//  too short body
		return false;
	}

	if(header->message_len>_MSG_SIZE)
	{
		// message too long
		return false;
	}
	
	if(header->header_len+header->body_len+header->data_len!=header->message_len)
	{
		// invalid lengths
		return false;
	}

	if(header->message_type!=type)
	{
		// wrong message type
		return false;
	}

	return true;
}

bool verifySessionId(char* msg, DWORD session)
{
	REQUEST_HEADER* header=(REQUEST_HEADER*)msg;

	return (header->session_id==session);
}

DWORD WINAPI StartSession(LPVOID param)
{
	DWORD err=1;
	DWORD sess=(DWORD)param;
	char request[_MSG_SIZE];
	client_data client;

	client.socket=sockets[sess];
	client.auth_data=NULL;
	client.auth_type=AUTH_NO_AUTH;
	client.keepalive=_TIMEOUT;
	client.session=session+sess;
	for(int i=0; i<=CAPI_MAXAPPL; i++) client.registered_apps[i]=0;


	while(run==1)
	{
		if(pause!=1)
		{
/*			fd_set test;
			FD_ZERO(&test);
			FD_SET(client.socket,&test);
			
			timeval time;
			time.tv_sec=0;
			time.tv_usec=500;

			if(select(0,&test,NULL,NULL,&time)==SOCKET_ERROR){
				DebugOut("StartSession(): select",FALSE);
				break;				
			}

			if(!FD_ISSET(client.socket,&test))
			{
				// no data pending
				continue;
			}*/

			err=recv(client.socket,request,_MSG_SIZE,0);	// blocking call
			if(err==0 || err==SOCKET_ERROR)
			{
				break;
			}

			if(debug) DebugOut("ReceiverRequest(): Received Request",FALSE);

			if(!verifyRequest(request))		// verify sizes
			{
				DebugOut("StartSession(): verifyRequest failed",FALSE);
				continue;
			}


			REQUEST_HEADER *header=(REQUEST_HEADER*)request;
			LPVOID body=(LPVOID)(request+header->header_len);
			char* data=request+header->header_len+header->body_len;

			switch(header->message_type)
			{
			case TYPE_CAPI_REGISTER:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_REGISTER",FALSE);
				if(verifySessionId(request,session+sess))
				{
					if(exec_capi_register(header,(REQUEST_CAPI_REGISTER*)body,data,&client)==SOCKET_ERROR)
					{
						DebugOut("ReceiveRequest(): sending answer failed",FALSE);
					}
				} else {
					DebugOut("ReceiveRequest(): verifySessionId() failed",FALSE);
				}
				break;

			case TYPE_CAPI_RELEASE:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_RELEASE",FALSE);
				if(verifySessionId(request,session+sess))
				{
					if(exec_capi_release(header,(REQUEST_CAPI_RELEASE*)body,data,&client)==SOCKET_ERROR)
					{
						DebugOut("ReceiveRequest(): sending answer failed",FALSE);
					}
				} else {
					DebugOut("ReceiveRequest(): verifySessionId() failed",FALSE);
				}
				break;

			case TYPE_CAPI_PUTMESSAGE:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_PUTMESSAGE",FALSE);
				if(verifySessionId(request,session+sess))
				{
					if(exec_capi_putmessage(header,(REQUEST_CAPI_PUTMESSAGE*)body,data,&client)==SOCKET_ERROR)
					{
						DebugOut("ReceiveRequest(): sending answer failed",FALSE);
					}
				} else {
					DebugOut("ReceiveRequest(): verifySessionId() failed",FALSE);
				}
				break;

			case TYPE_CAPI_GETMESSAGE:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_GETMESSAGE",FALSE);
				if(verifySessionId(request,session+sess))
				{
					if(exec_capi_getmessage(header,(REQUEST_CAPI_GETMESSAGE*)body,data,&client)==SOCKET_ERROR)
					{
						DebugOut("ReceiveRequest(): sending answer failed",FALSE);
					}
				} else {
					DebugOut("ReceiveRequest(): verifySessionId() failed",FALSE);
				}
				break;

			case TYPE_CAPI_WAITFORSIGNAL:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_WAITFORSIGNAL",FALSE);
				if(verifySessionId(request,session+sess))
				{
					if(exec_capi_waitforsignal(header,(REQUEST_CAPI_WAITFORSIGNAL*)body,data,&client)==SOCKET_ERROR)
					{
						DebugOut("ReceiveRequest(): sending answer failed",FALSE);
					}
				} else {
					DebugOut("ReceiveRequest(): verifySessionId() failed",FALSE);
				}
				break;

			case TYPE_CAPI_MANUFACTURER:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_MANUFACTURER",FALSE);
				if(verifySessionId(request,session+sess))
				{
					if(exec_capi_manufacturer(header,(REQUEST_CAPI_MANUFACTURER*)body,data,&client)==SOCKET_ERROR)
					{
						DebugOut("ReceiveRequest(): sending answer failed",FALSE);
					}
				} else {
					DebugOut("ReceiveRequest(): verifySessionId() failed",FALSE);
				}
				break;

			case TYPE_CAPI_VERSION:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_VERSION",FALSE);
				if(verifySessionId(request,session+sess))
				{
					if(exec_capi_version(header,(REQUEST_CAPI_VERSION*)body,data,&client)==SOCKET_ERROR)
					{
						DebugOut("ReceiveRequest(): sending answer failed",FALSE);
					}
				} else {
					DebugOut("ReceiveRequest(): verifySessionId() failed",FALSE);
				}
				break;

			case TYPE_CAPI_SERIAL:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_SERIAL",FALSE);
				if(verifySessionId(request,session+sess))
				{
					if(exec_capi_serial(header,(REQUEST_CAPI_SERIAL*)body,data,&client)==SOCKET_ERROR)
					{
						DebugOut("ReceiveRequest(): sending answer failed",FALSE);
					}
				} else {
					DebugOut("ReceiveRequest(): verifySessionId() failed",FALSE);
				}
				break;

			case TYPE_CAPI_PROFILE:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_PROFILE",FALSE);
				if(verifySessionId(request,session+sess))
				{
					if(exec_capi_profile(header,(REQUEST_CAPI_PROFILE*)body,data,&client)==SOCKET_ERROR)
					{
						DebugOut("ReceiveRequest(): sending answer failed",FALSE);
					}
				} else {
					DebugOut("ReceiveRequest(): verifySessionId() failed",FALSE);
				}
				break;

			case TYPE_CAPI_INSTALLED:
				if(debug) DebugOut("ReceiveRequest(): TYPE_CAPI_INSTALLED",FALSE);
				if(verifySessionId(request,session+sess))
				{
					if(exec_capi_installed(header,(REQUEST_CAPI_INSTALLED*)body,data,&client)==SOCKET_ERROR)
					{
						DebugOut("ReceiveRequest(): sending answer failed",FALSE);
					}
				} else {
					DebugOut("ReceiveRequest(): verifySessionId() failed",FALSE);
				}
				break;

			case TYPE_PROXY_HELO:
				if(debug) DebugOut("ReceiveRequest(): TYPE_PROXY_HELO",FALSE);
				if(exec_proxy_helo(header,(REQUEST_PROXY_HELO*)body,data,&client)==SOCKET_ERROR)
				{
					DebugOut("ReceiveRequest(): sending answer failed",FALSE);
				}
				break;

			case TYPE_PROXY_KEEPALIVE:
				if(debug) DebugOut("ReceiveRequest(): TYPE_PROXY_KEEPALIVE",FALSE);
				if(verifySessionId(request,session+sess))
				{
					if(exec_proxy_keepalive(header,(REQUEST_PROXY_KEEPALIVE*)body,data,&client)==SOCKET_ERROR)
					{
						DebugOut("ReceiveRequest(): sending answer failed",FALSE);
					}
				} else {
					DebugOut("ReceiveRequest(): verifySessionId() failed",FALSE);
				}
				break;

			case TYPE_PROXY_AUTH:
				if(debug) DebugOut("ReceiveRequest(): TYPE_PROXY_AUTH",FALSE);
				if(verifySessionId(request,session+sess))
				{
					if(exec_proxy_auth(header,(REQUEST_PROXY_AUTH*)body,data,&client)==SOCKET_ERROR)
					{
						DebugOut("ReceiveRequest(): sending answer failed",FALSE);
					}
				} else {
					DebugOut("ReceiveRequest(): verifySessionId() failed",FALSE);
				}
				break;

			default:
				DebugOut("ReceiveRequest(): Invalid Message Type",FALSE);
			}
		} else {
			Sleep(100);
		}
	}

	if(err==0)
	{
		exec_proxy_shutdown(&client,"Connection shutting down");
	} else {
		exec_proxy_shutdown(&client,"Socket Error");
	}

	// now we release all registered Apps
	DWORD app;

	while(app=getfirst_app(client.registered_apps))
	{
		CAPI_RELEASE(app);
	}

	closesocket(client.socket);
	freeSession(sess);

	return NO_ERROR;
}	
 
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

	sockaddr_in local;
	local.sin_family=AF_INET;
	local.sin_addr=localaddr;
	local.sin_port=htons((unsigned short)PORT);

	if(bind(sock,(SOCKADDR*)&local,sizeof(SOCKADDR_IN))==SOCKET_ERROR){
		DWORD err=WSAGetLastError();
		SetLastError(err);
		return INVALID_SOCKET;
	}
	return sock;
}

DWORD ServiceExit(){
    run=0;
	pause=0;

	int i;

	for(i=0; i<_MAX_SESSIONS; i++)
	{
		shutdown(sockets[i],SD_RECEIVE);
	}

	shutdown(socke,SD_RECEIVE);
	if(WaitForSingleObject(RunningThread,5000)!=WAIT_OBJECT_0){
		DebugOut("ExitThread(): WaitForSingleObject");
		closesocket(socke);

		TerminateThread(RunningThread,ERROR_BUSY);
	}
	CloseHandle(RunningThread);
	closesocket(socke);

	for(i=0; i<_MAX_SESSIONS; i++)
	{
		closesocket(sockets[i]);
	}

	Sleep(1000);

	return 0;
}

