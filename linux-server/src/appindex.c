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

struct appl_list* registered_apps;


void add_app(unsigned long  _ApplID, struct appl_list** _list)
{
	if(*_list!=NULL)
	{
		struct appl_list* list=(*_list);
		while(list->next)
		{
			list=list->next;
		}

		list->next=(struct appl_list*)malloc(sizeof(struct appl_list));

		list=list->next;
		list->ApplID=_ApplID;
		list->next=NULL;
	} else {
		(*_list)=(struct appl_list*)malloc(sizeof(struct appl_list));
		(*_list)->ApplID=_ApplID;
		(*_list)->next=NULL;
	}
}

void del_app(unsigned long _ApplID, struct appl_list** _list)
{
	if((*_list)!=NULL)
	{
		struct appl_list* list=(*_list);
		struct appl_list* prev=NULL;

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
				free((void*)list);
				return;
			}
			prev=list;
			list=list->next;
		}
	}
}

unsigned long getfirst_app(struct appl_list** _list)
{
	if((*_list)==NULL)
	{
		return 0;
	} else {
		struct appl_list* list=(*_list);
		unsigned long ret=list->ApplID;

		(*_list)=list->next;

		free((void*)list);
		return ret;
	}

}

void release_all(struct appl_list** _list)
{
	unsigned long app;
	while((app=getfirst_app(_list)))
	{
		capi20_release(app);
	}
}
