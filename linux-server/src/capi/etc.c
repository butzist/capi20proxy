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
// Some rest.
//
//////////////////////////////////////////////////////////////////////////////


#include "../capifwd.h"



void SET_CTRL(char* _msg, unsigned char _ctrl)
{
    unsigned long *ncci=&(CAPI_NCCI(_msg));
    *ncci &=  0xFFFFFF80;
    *ncci += _ctrl;
}
