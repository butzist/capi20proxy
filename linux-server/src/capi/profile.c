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


///////////// PROFILE ///////////////////////////////////////////////////////////////////////////////

int exec_capi_profile(void *in_packet) {
	struct REQUEST_HEADER *request;     // the request header
	struct ANSWER_CAPI_PROFILE *body;
	struct ANSWER_HEADER *head;

	request = (struct REQUEST_HEADER *) in_packet;
	body = (struct ANSWER_CAPI_PROFILE*) (out_packet + sizeof(struct ANSWER_HEADER));
	head = (struct ANSWER_HEADER*) out_packet;

	// Step 2: execute command
	head->capi_error = capi20_get_profile ( request->controller_id, body->profile );

	// Step 3: compose return header
	head->header_len = sizeof (struct ANSWER_HEADER);
	head->body_len = sizeof (struct ANSWER_CAPI_PROFILE);
	head->data_len = 0;
	head->message_len = head->header_len + head->body_len + head->data_len;
	head->message_id = request->message_id;
	head->message_type = TYPE_CAPI_PROFILE;
	head->app_id = request->app_id;
	head->session_id = sessionID;
	head->proxy_error = 0;

	pthread_mutex_lock(&smtx);
	numbytes = send( sock, out_packet, head->message_len, 0);
	pthread_mutex_unlock(&smtx);

	return numbytes;
}

///////////// END OF PROFILE /////////////////////////////////////////////////////////////////////////

