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

///////////// PUTMESSAGE ///////////////////////////////////////////////////////////////////////

int exec_capi_putmessage(void *in_packet) {
	struct REQUEST_HEADER *request;     // the request header
	struct ANSWER_HEADER *head;
	struct ANSWER_CAPI_PUTMESSAGE *body;
	char* msg;

	request = (struct REQUEST_HEADER*) in_packet;
	head = (struct ANSWER_HEADER*) out_packet;
	body = (struct ANSWER_CAPI_PUTMESSAGE*) ( out_packet + sizeof(struct ANSWER_HEADER));
	msg = ((char*)in_packet)+request->header_len+request->body_len;

	head->header_len = sizeof (struct ANSWER_HEADER);
	head->body_len = sizeof (struct ANSWER_CAPI_PUTMESSAGE);
	head->data_len = 0;
	head->message_len = head->header_len + head->body_len + head->data_len;
	head->message_id = request->message_id;
	head->message_type = TYPE_CAPI_PUTMESSAGE;
	head->app_id = request->app_id;
	head->session_id = sessionID;
	head->proxy_error = 0;





	// taken from win32 server
	if(request->data_len<8) // data too short
	{
		head->proxy_error=PERROR_PARSE_ERROR;
		syslog( LOG_WARNING, "Error parsing CAPI message (1)");
	} else {
		unsigned int len1=0;
		memcpy(&len1,msg,2);		// get the length of the CAPI message

		if(request->data_len<len1) // data too short
		{
			head->proxy_error=PERROR_PARSE_ERROR;
			syslog( LOG_WARNING, "Error parsing CAPI message (2)");
		} else {
			unsigned char cmd1=CAPI_CMD1(msg);
			unsigned char cmd2=CAPI_CMD2(msg);

			if((cmd1==0x86) && (cmd2==0x80))	// If we got a DATA_B3 REQ
			{
				if(len1<18)
				{
					head->proxy_error=PERROR_PARSE_ERROR;
					syslog( LOG_WARNING, "Error parsing CAPI message (3)");
				} else {
                			unsigned int len2=0;
					memcpy(&len2,msg+16,2);	// get the length of the data

					if(len1+len2>request->data_len)
					{
						head->proxy_error=PERROR_PARSE_ERROR;
						syslog( LOG_WARNING, "Error parsing CAPI message (4)");
					} else {
						unsigned long pointer=(unsigned long)msg+len1;		// data is appended to the message
						memcpy(msg+12,&pointer,4);

						// call the CAPI function
						head->capi_error=capi20_put_message ( request->app_id, msg );
					}
				}
			} else {
				// call the CAPI function
				head->capi_error=capi20_put_message ( request->app_id, msg );
			}
		}
	}

	numbytes = send( sock, out_packet, head->message_len, 0);
	return numbytes;
}

///////////// END OF PUTMESSAGE ////////////////////////////////////////////////////////////////

