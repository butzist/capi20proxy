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

//////////////////////// MANUFACTURER ////////////////////////////////////////////////////////

int exec_capi_manufacturer(void *in_packet) {
	struct REQUEST_HEADER *request;     // the request header
	struct ANSWER_CAPI_MANUFACTURER *body;
	struct ANSWER_HEADER *head;

	request = (struct REQUEST_HEADER *) in_packet;
	body = (struct ANSWER_CAPI_MANUFACTURER*) (out_packet + sizeof(struct ANSWER_HEADER));

	// Step 2: execute command
	capi20_get_manufacturer ( request->controller_id, body->manufacturer );

	// Step 3: compose return header
	head = (struct ANSWER_HEADER*) out_packet;
	head->header_len = sizeof (struct ANSWER_HEADER);
	head->body_len = sizeof (struct ANSWER_CAPI_MANUFACTURER);
	head->data_len = 0;
	head->message_len = head->header_len + head->body_len + head->data_len;
	head->message_id = request->message_id;
	head->message_type = TYPE_CAPI_MANUFACTURER;
	head->app_id = request->app_id;
	head->session_id = sessionID;
	head->proxy_error = 0;
	head->capi_error = 0x0000;

	
	numbytes = send( sock, out_packet, head->message_len, 0);
	return numbytes;


}

////////////////////// END OF MANUFACTURER /////////////////////////////////////////////////////
