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
// This file contains the CAPI-related functions.
// Each function queries the CAPI and then sends of a response packet.
//
//////////////////////////////////////////////////////////////////////////////


#include "../capifwd.h"

///////////// waitforsignal ///////////////////////////////////////////////////////////////////

struct listener_data
{
	unsigned long app_id;
	unsigned long message_id;
};


int exec_capi_waitforsignal(void *in_packet) {
	struct REQUEST_HEADER *request;
	struct listener_data data;
	struct ANSWER_HEADER *head;
	struct ANSWER_CAPI_WAITFORSIGNAL *body;
	pid_t pid;

	head = (struct ANSWER_HEADER*) out_packet;
	body = (struct ANSWER_CAPI_WAITFORSIGNAL*) (out_packet + sizeof(struct ANSWER_HEADER));

	request = (struct REQUEST_HEADER *) in_packet;
	numbytes=1;
	data.app_id=request->app_id;
	data.message_id=request->message_id;

	pid=fork();

	switch(pid)
	{
	case 0:
		head->capi_error = capi20_waitformessage( data.app_id, NULL );
		head->proxy_error=PERROR_NONE;
		break;
	case -1:
		syslog(LOG_NOTICE,"forking failed: %s", sys_errlist[errno]);
		head->capi_error= 0x1108;
		head->proxy_error=PERROR_NO_RESSOURCES;
		break;
	default:
		return 1;
	}

	head->header_len = sizeof (struct ANSWER_HEADER);
	head->body_len = sizeof (struct ANSWER_CAPI_WAITFORSIGNAL);
	head->data_len = 0;
	head->message_len = head->header_len + head->body_len + head->data_len;
	head->message_id = data.message_id;
	head->message_type = TYPE_CAPI_WAITFORSIGNAL;
	head->app_id = data.app_id;
	head->session_id = sessionID;

	numbytes=send( sock, out_packet, head->message_len, 0);
	
	if(pid==0) {
		exit(0);
	} else {
		return numbytes;
	}
}

////////////// END OF waitforsignal ///////////////////////////////////////////////////////////

