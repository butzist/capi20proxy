//////////////////////////////////////////////////////////////////////////////
// CAPI 2.0 Proxy Project:
//   capifwd server for linux.
// Copyright(C) 2002: F. Lindenberg, A. Szalkowski, B. Gerald.
//
// This program is free software following the terms of the GNU GPL.
// please refer to http://www.gnu.org/licenses/gpl.html .
//
// Support can be obtained from the capi20proxy-public@lists.sourceforge.net
// mailing lists.
//
// For this module, also contact: frlind@frlind.de. Please put capi20proxy
// somewhere in your Subject:-Line.
//////////////////////////////////////////////////////////////////////////////
//
// Authentication sub-system
//
//////////////////////////////////////////////////////////////////////////////


#include "../capifwd.h"

/* 
 * This function should return -1 for an authentication failure and any other value 
 * for success 
 */

int up_auth(char* indata)
{
	struct AUTH_USERPASS_DATA *info;
	char *uname;
	char *passwd;
	
	info = (struct AUTH_USERPASS_DATA*) indata;
	
	uname = (char*) malloc( info->uname_len );
	passwd = (char*) malloc( info->passwd_len );

	memcpy ( uname, indata + sizeof(struct AUTH_USERPASS_DATA), info->uname_len );
	memcpy ( passwd, indata + sizeof(struct AUTH_USERPASS_DATA) + info->uname_len, info->passwd_len );

	// Now check username and password ...
	
	return -1;
}

int ip_auth()
{
	int len=sizeof(struct sockaddr_in);
	struct sockaddr_in sockinfo;

	if(getsockname(sock,(struct sockaddr*)&sockinfo,&len)!=0) {
   		syslog ( LOG_WARNING, sys_errlist[errno] );
		return -1;
	}

	if(((ntohl(sockinfo.sin_addr.s_addr) & 0xFFFF0000) == 0xC0A80000)		//192.168.x.x
		|| ((ntohl(sockinfo.sin_addr.s_addr) & 0xFF000000) == 0x0A000000)	//10.x.x.x
		) {
		return 0;
	}

	return -1;
}
