
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

////////////// REGISTER /////////////////////////////////////////////////////////////////////////////

int exec_capi_register(void *in_packet) {
	struct REQUEST_HEADER *request;     // the request header
	//struct ANSWER_CAPI_REGISTER *body;
	struct REQUEST_CAPI_REGISTER *values;
	struct ANSWER_HEADER *head;
	unsigned long return_type;

	// Step 1: Parse incoming package
	request = (struct REQUEST_HEADER *) in_packet;
	values = (struct REQUEST_CAPI_REGISTER*) (in_packet + request->header_len);
	head = (struct ANSWER_HEADER*) out_packet;

	// Step 2: execute command
	return_type = capi20_register ( values->maxLogicalConnection, values->maxBDataBlocks, values->maxBDataLen, (unsigned int*) &(head->app_id) );
	if(return_type==0x0000)
	{
		add_app(head->app_id,registered_apps);
	}

	client_info.messageBufferLen = values->messageBufferSize;

	// Step 3: compose return header
	head->header_len = sizeof (struct ANSWER_HEADER);
	head->body_len = sizeof (struct ANSWER_CAPI_REGISTER);
	head->data_len = 0;
	head->message_len = head->header_len + head->body_len + head->data_len;
	head->message_id = request->message_id;
	head->message_type = TYPE_CAPI_REGISTER;
	head->session_id = sessionID;
	head->capi_error = return_type;
	head->proxy_error = 0;

	numbytes = send( sock, out_packet, head->message_len, 0);
	return numbytes;
}

///////////// END OF REGISTER //////////////////////////////////////////////////////////////////////

