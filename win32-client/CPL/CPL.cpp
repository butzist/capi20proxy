// CPL.cpp : Definiert den Einsprungpunkt für die DLL-Anwendung.
//

#include "stdafx.h"
#include "resource.h"

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
    }
    return TRUE;
}

static int WINAPI handler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
	case WM_INITDIALOG:
		{
			HKEY key;
			char server[256];
			char name[64];
			char str_port[10];
			unsigned char ip1,ip2,ip3,ip4;
			DWORD port,ip,err,len;

			err=RegOpenKeyEx(HKEY_LOCAL_MACHINE,"Software\\The Red Guy\\capi20proxy",0,KEY_QUERY_VALUE,&key);
			if(err!=ERROR_SUCCESS){
				::MessageBox(NULL,"Could not open Registry Key \"HKEY_LOCAL_MACHINE\\Software\\The Red Guy\\capi20proxy\"","Error",MB_OK);
				break;
			}
			
			len=256;
			err=RegQueryValueEx(key,"server",NULL,NULL,(unsigned char*)server,&len);
			if(err!=ERROR_SUCCESS){
				strcpy(server,"10.0.0.1");
			}

			sscanf(server,"%d.%d.%d.%d",&ip1,&ip2,&ip3,&ip4);	// WOW! it works!
			ip=MAKEIPADDRESS(ip1,ip2,ip3,ip4);
			SendMessage(
				GetDlgItem(hwnd,IDC_IPADDRESS),
				IPM_SETADDRESS,
				0,
				(LPARAM)ip);

			len=64;
			err=RegQueryValueEx(key,"name",NULL,NULL,(unsigned char*)name,&len);
			if(err!=ERROR_SUCCESS){
				strcpy(name,"Freddy");
			}

			SendMessage(
				GetDlgItem(hwnd,IDC_NAME),
				WM_SETTEXT,
				0,
				(LPARAM)name);


			len=4;
			err=RegQueryValueEx(key,"port",NULL,NULL,(unsigned char*)&port,&len);
			if(err!=ERROR_SUCCESS){
				port=6674;
			}

			_itoa(port,str_port,10);
			SendMessage(
				GetDlgItem(hwnd,IDC_PORT),
				WM_SETTEXT,
				0,
				(LPARAM)str_port);

			RegCloseKey(key);
		}
		return TRUE;

	case WM_CLOSE:
		EndDialog(hwnd,IDCANCEL);
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			{
				HKEY key;
				char server[256];
				char name[64];
				char str_port[10];
				unsigned char ip1,ip2,ip3,ip4;
				DWORD port,ip,err,len;

				err=RegOpenKeyEx(HKEY_LOCAL_MACHINE,"Software\\The Red Guy\\capi20proxy",0,KEY_QUERY_VALUE | KEY_SET_VALUE,&key);
				if(err!=ERROR_SUCCESS){
					::MessageBox(NULL,"Could not open Registry Key \"HKEY_LOCAL_MACHINE\\Software\\The Red Guy\\capi20proxy\"","Error",MB_OK);
					break;
				}
				
				SendMessage(
					GetDlgItem(hwnd,IDC_IPADDRESS),
					IPM_GETADDRESS,
					0,
					(LPARAM)&ip);

				ip1=(unsigned char)FIRST_IPADDRESS(ip);
				ip2=(unsigned char)SECOND_IPADDRESS(ip);
				ip3=(unsigned char)THIRD_IPADDRESS(ip);
				ip4=(unsigned char)FOURTH_IPADDRESS(ip);

				sprintf(server,"%d.%d.%d.%d",ip1,ip2,ip3,ip4);
				ip=MAKEIPADDRESS(ip1,ip2,ip3,ip4);

				err=RegSetValueEx(key,"server",NULL,REG_SZ,(unsigned char*)server,strlen(server));
				if(err!=ERROR_SUCCESS){
					::MessageBox(NULL,"Could not write Registry Value \"HKEY_LOCAL_MACHINE\\Software\\The Red Guy\\capi20proxy\\server\"","Error",MB_OK);
				}


				len=SendMessage(
					GetDlgItem(hwnd,IDC_NAME),
					WM_GETTEXTLENGTH,
					0,0);

				SendMessage(
					GetDlgItem(hwnd,IDC_NAME),
					WM_GETTEXT,
					(WPARAM)(len+1),
					(LPARAM)name);

				err=RegSetValueEx(key,"name",NULL,REG_SZ,(unsigned char*)name,strlen(name));
				if(err!=ERROR_SUCCESS){
					::MessageBox(NULL,"Could not write Registry Value \"HKEY_LOCAL_MACHINE\\Software\\The Red Guy\\capi20proxy\\name\"","Error",MB_OK);
				}

				len=SendMessage(
					GetDlgItem(hwnd,IDC_PORT),
					WM_GETTEXTLENGTH,
					0,0);
				
				SendMessage(
					GetDlgItem(hwnd,IDC_PORT),
					WM_GETTEXT,
					(WPARAM)(len+1),
					(LPARAM)str_port
					);

				port=atoi(str_port);

				err=RegSetValueEx(key,"port",NULL,REG_DWORD,(unsigned char*)&port,4);
				if(err!=ERROR_SUCCESS){
					::MessageBox(NULL,"Could not write Registry Value \"HKEY_LOCAL_MACHINE\\Software\\The Red Guy\\capi20proxy\\port\"","Error",MB_OK);
				}

				RegCloseKey(key);
			}
			EndDialog(hwnd,IDOK);
			break;
		case IDCANCEL:
			EndDialog(hwnd,IDCANCEL);
			break;

		default:
			break;
		}
		return TRUE;

	default:
		return FALSE;
	}
	return FALSE;
}

HINSTANCE hinst;

LONG CALLBACK CPlApplet(HWND hwndCPL, UINT uMsg, LPARAM lParam1, LPARAM lParam2) 
{ 
    int i; 
	INITCOMMONCONTROLSEX ictrl;
    LPCPLINFO lpCPlInfo; 
//	::MessageBox(NULL,"test","test",MB_OK);
 
    i = (int) lParam1; 
 
    switch (uMsg) { 
        case CPL_INIT:
            hinst = GetModuleHandle("capi20proxy.cpl"); 
            return TRUE; 
 
        case CPL_GETCOUNT:
            return 1; 
            break; 
 
        case CPL_INQUIRE:
            lpCPlInfo = (LPCPLINFO) lParam2; 
            lpCPlInfo->lData = 0; 
            lpCPlInfo->idIcon = IDI_ICON;
            lpCPlInfo->idName = IDS_NAME;
            lpCPlInfo->idInfo = IDS_DESC;
            break; 

        case CPL_DBLCLK:
			ictrl.dwSize=sizeof(INITCOMMONCONTROLSEX);
			ictrl.dwICC=ICC_INTERNET_CLASSES;
			InitCommonControlsEx(&ictrl);
			DialogBox(hinst, MAKEINTRESOURCE(IDD_MAINDIALOG), hwndCPL, handler);
            break; 
 
        case CPL_STOP:
            break; 
 
        case CPL_EXIT:
            break; 
 
        default: 
            break; 
    } 
    return 0; 
}
