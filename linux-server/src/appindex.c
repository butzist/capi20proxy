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
// This file contains Adam's code for application indexing.
// I don't see the particular sense in it since it also works without that
// code, but he wanted it soooo much... ;)
//
//////////////////////////////////////////////////////////////////////////////

#include "capifwd.h"

char registered_apps[CAPI_MAXAPPL];


void add_app(unsigned long  _ApplID, char* _list)
{
	_list[_ApplID]=1;
}

void del_app(unsigned long _ApplID, char* _list)
{
	_list[_ApplID]=0;	
}

unsigned long getfirst_app(char* _list)
{
	int i;
	for(i=1; i<=CAPI_MAXAPPL; i++) {
		if(_list[i]!=0) {
			_list[i]=0;
			return i;
		}
	}
	return 0;
}

void release_all(char* _list)
{
	unsigned long app;
	while((app=getfirst_app(_list)))
	{
		capi20_release(app);
	}
}



