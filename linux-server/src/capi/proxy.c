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

//////////////// HELO //////////////////////////////////////////////////////////////////////////////////

int exec_proxy_helo(void *in_packet) {
	struct REQUEST_HEADER *request;     // the request header
	struct REQUEST_PROXY_HELO *helo_msg;
	struct ANSWER_PROXY_HELO *helo_re;
	struct ANSWER_HEADER *helo_head;

	// Step 1: extract client information...
	request = (struct REQUEST_HEADER *) in_packet;
	helo_msg = (struct REQUEST_PROXY_HELO*) ( in_packet + request->header_len );
	client_info.os = helo_msg->os;
	client_info.version = helo_msg->version;
	strcpy(client_info.name, helo_msg->name);
	// Step 2: compose answer header...
	helo_head = (struct ANSWER_HEADER*) out_packet;
	helo_head->message_len = (sizeof( struct ANSWER_HEADER ) + sizeof (struct  ANSWER_PROXY_HELO ));
	helo_head->header_len = sizeof( struct ANSWER_HEADER );
	helo_head->body_len = sizeof( struct ANSWER_PROXY_HELO );
	helo_head->data_len = 0;
	helo_head->message_id = request->message_id;
	helo_head->message_type = TYPE_PROXY_HELO;
	helo_head->app_id = 0;
	helo_head->session_id = sessionID;
	helo_head->proxy_error = 0;
	helo_head->capi_error = 0;
	// Step 3: compose answer packet...
	helo_re = (struct ANSWER_PROXY_HELO*) (out_packet + sizeof (struct ANSWER_HEADER));
	strcpy ( helo_re->name,  "CAPI 2.0 Proxy Linux SERVER pre-alpha");
	helo_re->os = OS_TYPE_LINUX;
	helo_re->version = local_version;
	helo_re->auth_type = AUTH_NO_AUTH;		//Authentification not yet supported.

	numbytes = send( sock, out_packet, helo_head->message_len, 0);
	return numbytes;
}

//////////////// END OF HELO //////////////////////////////////////////////////////////////////////////


/////////////// AUTH /////////////////////////////////////////////////////////////////////////////////

int exec_proxy_auth(void *in_packet) {
	struct REQUEST_HEADER *request;     // the request header
	struct REQUEST_PROXY_AUTH *lenp;
	struct ANSWER_PROXY_AUTH *body;
	struct ANSWER_HEADER *head;
	char *authdata;
	int noverfy=-1;
	
	lenp = (struct REQUEST_PROXY_AUTH*) (in_packet+sizeof(struct REQUEST_HEADER));
	if ( lenp->auth_len != 0 ) {
		authdata = (char*) malloc( lenp->auth_len * sizeof(char));
		memcpy (authdata, (in_packet + sizeof(struct REQUEST_HEADER) + sizeof( struct REQUEST_PROXY_AUTH)), lenp->auth_len );
		noverfy = up_auth(authdata);
	}
 
	
	// Step 3: compose return header
	request = (struct REQUEST_HEADER *) in_packet;
	head = (struct ANSWER_HEADER*) out_packet;
	head->header_len = sizeof (struct ANSWER_HEADER);
	head->body_len = sizeof (struct ANSWER_PROXY_AUTH);
	head->data_len = 0;
	head->message_len = head->header_len + head->body_len + head->data_len;
	head->message_id = request->message_id;
	head->message_type = TYPE_PROXY_AUTH;
	head->app_id = 0;
	if ( noverfy == -1 ) {
		head->session_id = 0;
	}
	else {
		head->session_id = sessionID;
	}
	head->capi_error = 0x0000; //return_type;
	head->proxy_error = PERROR_AUTH_TYPE_NOT_SUPPORTED;

	// Step 4: compose return body
	body = (struct ANSWER_PROXY_AUTH*) (out_packet + sizeof(struct ANSWER_HEADER));
	body->auth_type=0;
	body->auth_len=0;

	numbytes = send( sock, out_packet, head->message_len, 0);
	return numbytes;
}

////////////// END OF AUTH /////////////////////////////////////////////////////////////////////////



////////////// PROXYSHUTDOWN ////////////////////////////////////////////////////////////////////////

int exec_proxy_shutdown(char *excuse) {
	struct ANSWER_PROXY_SHUTDOWN *sdown_re;
	struct ANSWER_HEADER *sdown_head;


	// Step 1: Compose header....
	sdown_head = (struct ANSWER_HEADER*) out_packet;
	sdown_head->message_len = (sizeof( struct ANSWER_HEADER ) + sizeof ( struct ANSWER_PROXY_SHUTDOWN ));
	sdown_head->header_len = sizeof( struct ANSWER_HEADER );
	sdown_head->body_len = sizeof( struct ANSWER_PROXY_SHUTDOWN );
	sdown_head->data_len = 0;
	sdown_head->message_id = 0;
	sdown_head->message_type = TYPE_PROXY_SHUTDOWN;
	sdown_head->app_id = 0;
	sdown_head->session_id = sessionID;
	sdown_head->proxy_error = 0;
	sdown_head->capi_error = 0;

	// Step 2: Compose body ...
	sdown_re = (struct ANSWER_PROXY_SHUTDOWN*) (out_packet + sizeof(struct ANSWER_HEADER));
	strcpy ( sdown_re->reason, excuse );

	numbytes = send( sock, out_packet, sdown_head->message_len, 0);
	return numbytes;
}

////////////// END OF PROXYSHUTDOWN /////////////////////////////////////////////////////////////////
