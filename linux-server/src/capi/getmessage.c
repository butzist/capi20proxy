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

///////////// GETMESSAGE ///////////////////////////////////////////////////////////////////////

int exec_capi_getmessage(void *in_packet) {
	struct REQUEST_HEADER *request;     // the request header
	struct ANSWER_HEADER *head;
	struct ANSWER_CAPI_GETMESSAGE *body;
	char *message, *data;

	request = (struct REQUEST_HEADER *) in_packet;
	head = (struct ANSWER_HEADER*) out_packet;
	body = (struct ANSWER_CAPI_GETMESSAGE*) (out_packet + sizeof(struct ANSWER_HEADER));
	data = (char*)(out_packet + sizeof(struct ANSWER_HEADER) + sizeof(struct ANSWER_CAPI_GETMESSAGE));

	// call the capi
	head->capi_error = capi20_get_message(request->app_id,(unsigned char**) &message);

	// compose the answer packet
	head->header_len = sizeof (struct ANSWER_HEADER);
	head->body_len = sizeof (struct ANSWER_CAPI_GETMESSAGE);
	head->message_id = request->message_id;
	head->data_len=0;
	head->message_type = TYPE_CAPI_GETMESSAGE;
	head->app_id = request->app_id;
	head->session_id = sessionID;
	head->proxy_error = 0;

	// this part was copied from the win32 server
	if(head->capi_error==0x0000)
	{
		unsigned int len1=0;
		unsigned char cmd1,cmd2;

		memcpy(&len1,message,2);		// get the length of the CAPI message

		memcpy(data,message,len1);		// copy the message to the answer
		head->data_len=len1;

		cmd1=CAPI_CMD1(data);
		cmd2=CAPI_CMD2(data);

		if((cmd1==0x86) && (cmd2==0x82))	// If we got a DATA_B3 IND
		{
			if(len1<18)
			{
				head->proxy_error=PERROR_PARSE_ERROR;
			} else {
            			unsigned int len2=0,pointer=0;
				memcpy(&len2,data+16,2);	// get the length of the data

				memcpy(&pointer,data+12,4);	// get the start address of the data

				memcpy(data+len1,(void*)pointer,len2);	// append the data to the CAPI message

				head->data_len+=len2;
			}
		}
	}

	head->message_len = head->header_len + head->body_len + head->data_len;

	pthread_mutex_lock(&smtx);
	numbytes = send( sock, out_packet, head->message_len, 0);
	pthread_mutex_unlock(&smtx);

	return numbytes;
}

///////////// END OF GETMESSAGE ////////////////////////////////////////////////////////////////

