

/* CVS Log:
 * $Log$
 * Revision 1.14  2002/05/06 14:14:36  butzist
 * fixed the syntax errors
 * */

const char *id = "capiproxyd linux server by f. lindenberg & a. szalkowski";

// version:
#define MY_MAJOR 1
#define MY_MINOR 1


#define _GNU_SOURCE

#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <linux/capi.h>
#include <unistd.h>
#include <capi20.h>
#include <syslog.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>


#include "protocol.h"

#define __PORT 6674
#define __TIMEOUT 500
#define	_PACKET_SIZE 10000

#define APPL_ID(msg)	(*((unsigned int*)(((char*)msg)+2)))
#define CAPI_NCCI(msg)	(*((unsigned long*)(((char*)msg)+8)))
#define CAPI_CMD1(msg)	(*((unsigned char*)(((char*)msg)+4)))
#define CAPI_CMD2(msg)	(*((unsigned char*)(((char*)msg)+5)))

void inline SET_CTRL(char* _msg, unsigned char _ctrl)
{
    unsigned long *ncci=&(CAPI_NCCI(_msg));
    *ncci &=  0xFFFFFF80;
    *ncci += _ctrl;
}



//////////// NETWORKING VARIABLES //////////////////////////////////////////////////////////////

int sc;				    // local socket descriptor
int sock;			    // remote socket descriptor
struct sockaddr_in local_addr;      // local socket properties
struct sockaddr_in remote_addr;     // remote socket properties

char out_packet[_PACKET_SIZE];
int numbytes;

// capi time-out mechanism
int timeout;                        // time-out (in seconds)
time_t init;						// time of last packet

/////////// CAPI VARIABLES /////////////////////////////////////////////////////////////////////

unsigned long sessionID;                      // session id (assigned at each fork())
unsigned long auth_types;                  // index of supported auth. types

////////// List of registered apps /////////////////////////////////////////////////////////////
struct appl_list
{
	struct appl_list* next;
	unsigned long ApplID;
};

struct appl_list* registered_apps=NULL;

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

///////////////////////////////////////////////////////////////////////////////////

struct __version_t local_version;   // my local version
// the client info struct
struct _client_info {
	int os;							// client operating system
	struct __version_t version;		// client program version
	char name[64];					// client log-name
	int messageBufferLen;			// buffer length used by the client
} client_info;


//////////// MULTITHREADING CODE SINCE 0.3test1

// pthread_t hand;
 pthread_mutex_t smtx;
 pthread_mutex_t capimtx;

//////// END OF GLOBAL VARIABLES ////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////


int exec_capi_getmessage(void *in_packet);
int exec_capi_isinstalled(void *in_packet);
int exec_capi_manufacturer(void *in_packet);
int exec_capi_profile(void *in_packet);
int exec_capi_putmessage(void *in_packet);
int exec_capi_register(void *in_packet);
int exec_capi_release(void *in_packet);
int exec_capi_serial(void *in_packet);
int exec_capi_version(void *in_packet);
int exec_capi_waitforsignal(void *in_packet);
int exec_proxy_auth(void *in_packet);
int exec_proxy_helo(void *in_packet);
int exec_proxy_shutdown(char* excuse);


/////////////////////////////////////////////////////////////////////////////////////////////////
/////// QUIT: A SIMPLE CLOSE-THE-SOCKET EXIT FUNCTION
int quit( int ret ) {
	closelog();
	close ( sc );
	exit ( ret );
	return ret;
}

////// XQUIT: SEND A MESSAGE TO THE CLIENT BEFORE CALLING EXIT
int xquit ( int ret, char excuse[128]) {
	exec_proxy_shutdown(excuse);
	close(sock);
	exit(0);
	return 0;
}

///// VERIFY_SESSION_ID: CHECK THE CLIENTS SESSION ID
int verify_session_id( unsigned id )
{
	if ( id == sessionID ) {
		return 0;
	}
	return 1;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAPI RELATED FUNCTIONS /////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////


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

	pthread_mutex_lock(&smtx);
	numbytes = send( sock, out_packet, helo_head->message_len, 0);
	pthread_mutex_unlock(&smtx);

	return numbytes;
}

//////////////// END OF HELO //////////////////////////////////////////////////////////////////////////


/////////////// AUTH /////////////////////////////////////////////////////////////////////////////////

int exec_proxy_auth(void *in_packet) {
	struct REQUEST_HEADER *request;     // the request header
	struct ANSWER_PROXY_AUTH *body;
	struct ANSWER_HEADER *head;

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
	head->session_id = sessionID;
	head->capi_error = 0x0000; //return_type;
	head->proxy_error = PERROR_AUTH_TYPE_NOT_SUPPORTED;

	// Step 4: compose return body
	body = (struct ANSWER_PROXY_AUTH*) (out_packet + sizeof(struct ANSWER_HEADER));
	body->auth_type=0;
	body->auth_len=0;

	pthread_mutex_lock(&smtx);
	numbytes = send( sock, out_packet, head->message_len, 0);
	pthread_mutex_unlock(&smtx);

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

	pthread_mutex_lock(&smtx);
	numbytes = send( sock, out_packet, sdown_head->message_len, 0);
	pthread_mutex_unlock(&smtx);

	return numbytes;
}

////////////// END OF PROXYSHUTDOWN /////////////////////////////////////////////////////////////////


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
		add_app(head->app_id,&registered_apps);
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

	pthread_mutex_lock(&smtx);
	numbytes = send( sock, out_packet, head->message_len, 0);
	pthread_mutex_unlock(&smtx);

	return numbytes;
}

///////////// END OF REGISTER //////////////////////////////////////////////////////////////////////


//////////// RELEASE ///////////////////////////////////////////////////////////////////////////////

int exec_capi_release(void *in_packet) {
	struct REQUEST_HEADER *request;     // the request header
	struct ANSWER_HEADER *head;
	unsigned long return_type;

	// Step 2: execute command
	request = (struct REQUEST_HEADER *) in_packet;
	return_type = capi20_release ( request->app_id );

	if(return_type==0x0000)
	{
		del_app(request->app_id,&registered_apps);
	}

	// Step 3: compose return header
	head = (struct ANSWER_HEADER*) out_packet;
	head->header_len = sizeof (struct ANSWER_HEADER);
	head->body_len = 0;
	head->data_len = 0;
	head->message_len = head->header_len + head->body_len + head->data_len;
	head->message_id = request->message_id;
	head->message_type = TYPE_CAPI_RELEASE;
	head->app_id = request->app_id;
	head->session_id = sessionID;
	head->capi_error = return_type;
	head->proxy_error = 0;

	// Step 4: compose return body

	pthread_mutex_lock(&smtx);
	numbytes = send( sock, out_packet, head->message_len, 0);
	pthread_mutex_unlock(&smtx);

	return numbytes;
}

///////////// END OF RELEASE ///////////////////////////////////////////////////////////////////


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

	pthread_mutex_lock(&smtx);
	numbytes = send( sock, out_packet, head->message_len, 0);
	pthread_mutex_unlock(&smtx);

	return numbytes;
}

///////////// END OF PUTMESSAGE ////////////////////////////////////////////////////////////////


///////////// waitforsignal ///////////////////////////////////////////////////////////////////

struct listener_data
{
	unsigned long app_id;
	unsigned long message_id;
};

/*void* lx_listen(void* param) {
	struct listener_data* data=(struct listener_data*)param;

	struct ANSWER_HEADER *head;
	struct ANSWER_CAPI_WAITFORSIGNAL *body;
	char out_packet[_PACKET_SIZE];
	int numbytes;

	head = (struct ANSWER_HEADER*) out_packet;
	body = (struct ANSWER_CAPI_WAITFORSIGNAL*) (out_packet + sizeof(struct ANSWER_HEADER));
	head->capi_error = capi20_waitformessage ( data->app_id, NULL);

	head->header_len = sizeof (struct ANSWER_HEADER);
	head->body_len = sizeof (struct ANSWER_CAPI_WAITFORSIGNAL);
	head->data_len = 0;
	head->message_len = head->header_len + head->body_len + head->data_len;
	head->message_id = data->message_id;
	head->message_type = TYPE_CAPI_WAITFORSIGNAL;
	head->app_id = data->app_id;
	head->session_id = sessionID;
	head->proxy_error=PERROR_NONE;

	free(param);

	pthread_mutex_lock(&smtx);
	numbytes = send( sock, out_packet, head->message_len, 0);
	pthread_mutex_unlock(&smtx);

	return (void*) numbytes;
}*/

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

	pthread_mutex_lock(&smtx);
	numbytes=send( sock, out_packet, head->message_len, 0);
	pthread_mutex_unlock(&smtx);

	if(pid==0)
	{
		exit(0);
	} else {
		return numbytes;
	}
}

////////////// END OF waitforsignal ///////////////////////////////////////////////////////////


////////////// INSTALLED ///////////////////////////////////////////////////////////////////////

int exec_capi_isinstalled(void *in_packet) {
	struct REQUEST_HEADER *request;     // the request header
	struct ANSWER_HEADER *head;
	unsigned long return_type;

	// Step 2: execute command
	request = (struct REQUEST_HEADER *) in_packet;
	return_type = capi20_isinstalled ( );

	// Step 3: compose return header
	head = (struct ANSWER_HEADER*) out_packet;
	head->header_len = sizeof (struct ANSWER_HEADER);
	head->body_len = 0;
	head->data_len = 0;
	head->message_len = head->header_len + head->body_len + head->data_len;
	head->message_id = request->message_id;
	head->message_type = TYPE_CAPI_INSTALLED;
	head->app_id = request->app_id;
	head->session_id = sessionID;
	head->capi_error = return_type;
	head->proxy_error = 0;

	// Step 4: compose return body
	pthread_mutex_lock(&smtx);
	numbytes = send( sock, out_packet, head->message_len, 0);
	pthread_mutex_unlock(&smtx);

	return numbytes;
}

///////////// END OF INSTALLED //////////////////////////////////////////////////////////////////////


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


///////////// SERIAL /////////////////////////////////////////////////////////////////////////////////

int exec_capi_serial(void *in_packet) {
	struct REQUEST_HEADER *request;     // the request header
	struct ANSWER_CAPI_SERIAL *body;
	struct ANSWER_HEADER *head;

	request = (struct REQUEST_HEADER *) in_packet;
	body = (struct ANSWER_CAPI_SERIAL*) (out_packet + sizeof(struct ANSWER_HEADER));
	head = (struct ANSWER_HEADER*) out_packet;

	// Step 2: execute command
	capi20_get_serial_number ( request->controller_id, body->serial );

	// Step 3: compose return header
	head->header_len = sizeof (struct ANSWER_HEADER);
	head->body_len = sizeof (struct ANSWER_CAPI_SERIAL);
	head->data_len = 0;
	head->message_len = head->header_len + head->body_len + head->data_len;
	head->message_id = request->message_id;
	head->message_type = TYPE_CAPI_SERIAL;
	head->app_id = request->app_id;
	head->session_id = sessionID;
	head->proxy_error = 0;
	head->capi_error = 0x0000;

	pthread_mutex_lock(&smtx);
	numbytes = send( sock, out_packet, head->message_len, 0);
	pthread_mutex_unlock(&smtx);

	return numbytes;
}

///////////// END OF SERIAL //////////////////////////////////////////////////////////////////////////


///////////// VERSION ////////////////////////////////////////////////////////////////////////////////

int exec_capi_version(void *in_packet) {
	struct REQUEST_HEADER *request;     // the request header
	struct ANSWER_CAPI_VERSION *body;
	struct ANSWER_HEADER *head;

	//unsigned long return_type;
	unsigned long buffer[4];
	request = (struct REQUEST_HEADER *) in_packet;
	body = (struct ANSWER_CAPI_VERSION*) (out_packet + sizeof(struct ANSWER_HEADER));

	// Step 2: execute command
	capi20_get_version ( request->controller_id, (char*) buffer );

	// Step 3: compose return header
	head = (struct ANSWER_HEADER*) out_packet;
	head->header_len = sizeof (struct ANSWER_HEADER);
	head->body_len = sizeof (struct ANSWER_CAPI_VERSION);
	head->data_len = 0;
	head->message_len = head->header_len + head->body_len + head->data_len;
	head->message_id = request->message_id;
	head->message_type = TYPE_CAPI_VERSION;
	head->app_id = request->app_id;
	head->session_id = sessionID;
	head->proxy_error = 0;
	head->capi_error=0x0000;

	body->driver.major=buffer[0];
	body->driver.minor=buffer[1];
	body->manufacturer.major=buffer[2];
	body->manufacturer.minor=buffer[3];


	pthread_mutex_lock(&smtx);
	numbytes = send( sock, out_packet, head->message_len, 0);
	pthread_mutex_unlock(&smtx);

	return numbytes;
}

//////////////////////// END OF VERSION //////////////////////////////////////////////////////


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

	// Step 4: compose return body
	pthread_mutex_lock(&smtx);
	numbytes = send( sock, out_packet, head->message_len, 0);
	pthread_mutex_unlock(&smtx);

	return numbytes;


}

////////////////////// END OF MANUFACTURER /////////////////////////////////////////////////////


// Signal handlers
// for main process
void sighandler1(int signal)
{
	switch(signal)
	{
	case SIGINT:
	case SIGTERM:
		close (sc);
		syslog(LOG_NOTICE,"got signal %s and exiting",sys_siglist[signal]);
		exit ( 0 );
		break;

	case SIGHUP:
	case SIGUSR1:
	default:
		syslog(LOG_NOTICE,"ignoring received signal: %s",sys_siglist[signal]);
	}
}

// for subprocesses
void sighandler2(int signal)
{
	switch(signal)
	{
	case SIGINT:
	case SIGTERM:
		close (sock);
		syslog(LOG_NOTICE,"got signal %s and exiting",sys_siglist[signal]);
		exit ( 0 );
		break;

	case SIGHUP:
	case SIGUSR1:
	default:
		syslog(LOG_NOTICE,"ignoring received signal: %s",sys_siglist[signal]);
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////// MAIN //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

int main ( int argc, char* argv[] ) {
	struct REQUEST_HEADER *request;     // the request header
   	char in_packet[_PACKET_SIZE];
  	char msg[50];
	int remote_size,numbytes,sentbytes;

	auth_types = AUTH_NO_AUTH;
	local_version.major = MY_MAJOR;
	local_version.minor = MY_MINOR;

	timeout = __TIMEOUT;

	// fork() so the parent can exit
	switch(fork())
	{
	case 0:  break;
	case -1: exit(1);
	default: exit(0);
	}

	// setsid() to become a group leader -> now no controlling terminal
	setsid();

	// fork() again so the session leader can exit
	// -> no regaining of terminal possible
	switch(fork())
	{
	case 0:  break;
	case -1: exit(1);
	default: exit(0);
	}

	// free used directory
	chdir("/");

	// we don't know what umask we inherited
	umask(0);

	// release fds
	fclose(stdin);
	fclose(stdout);
	fclose(stderr);
	fcloseall();

	// override some signals
	signal(SIGINT, sighandler1);
	signal(SIGHUP, sighandler1);
	signal(SIGTERM, sighandler1);
	signal(SIGUSR1, sighandler1);

  	openlog ("capi20proxy",LOG_PID, LOG_DAEMON);
  	syslog ( LOG_NOTICE, "capi20proxy server started.");

	if ( ( sc = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
   	syslog ( LOG_WARNING, "error during socket creation: capi20proxy exits.");
   	exit ( 1 );
  	}

  	local_addr.sin_family = AF_INET;
  	local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  	local_addr.sin_port = htons ( __PORT );

  if ( bind ( sc, (struct sockaddr*)&local_addr, sizeof(local_addr)) == -1 ) {
   syslog ( LOG_WARNING, "error during socket binding: capi20proxy exits");
   close ( sc );
   exit ( 1 );
  }

  if ( listen( sc, 3) == -1 ) {
  	syslog ( LOG_WARNING, "error during socket listening: capi20proxy exits");
	close (sc );
	exit ( 1 );
  }

  //fcntl(sc, F_SETFL, O_NONBLOCK);

  // rename the process: strange code, isn't it?
  strcpy ( argv[0], "CAPI20proxy: listen");
  remote_size = sizeof (struct sockaddr_in);
  sessionID = 0;
  while ( 1 ) {
		sock = accept ( sc, (struct sockaddr*)&remote_addr, &remote_size );

		while(waitpid(-1,NULL,WNOHANG)>0);
		if ( sock == -1 ) {
			syslog(LOG_WARNING,"network error, exiting.");
			close(sc);
			exit(1);
		}
		sessionID++;

		switch(fork())
		{
		case 0: break;
		case -1:
			syslog (LOG_WARNING, "forking failed, capi20proxy goes down!");
			close ( sc );
			close ( sock );
			exit ( 1 );
		default: continue;
		}

		sprintf(msg, "CAPI20proxy: handle %s", inet_ntoa(remote_addr.sin_addr));
		strcpy ( argv[0], msg);
		sprintf(msg, "incoming connection from: %s", inet_ntoa(remote_addr.sin_addr));
		syslog ( LOG_NOTICE, msg );
		init = time (NULL);
		pthread_mutex_init(&smtx,NULL);

		signal(SIGINT, sighandler2);
		signal(SIGHUP, sighandler2);
		signal(SIGTERM, sighandler2);
		signal(SIGUSR1, sighandler2);

		////////////////////////////////////////////////////////////////////////////////
		// Handling
		while ( 1 ) {
			while(waitpid(-1,NULL,WNOHANG)>0);
			if ((numbytes = recv( sock, in_packet, _PACKET_SIZE, 0)) < 1) {
				close (sock);
				release_all(&registered_apps);	//release the registered applications
				if(numbytes==0)
				{
					// normal shutdown when socket is closed
					syslog(LOG_NOTICE,"quitting normally");
					exit ( 0 );
				}
				else
				{
					syslog ( LOG_WARNING, "network error, quitting abnormally" );
					exit ( 1 );
				}
			}
			if ( numbytes > 0) {
				init = time ( NULL );
				sentbytes=1;
				request = (struct REQUEST_HEADER *) in_packet;
				switch ( request->message_type ) {
					case TYPE_PROXY_HELO:
						sentbytes=exec_proxy_helo((void*)in_packet);
						break;
					case TYPE_PROXY_AUTH:
						sentbytes=exec_proxy_auth((void*) in_packet);
						break;
					case TYPE_PROXY_KEEPALIVE:
						break;
					case TYPE_CAPI_REGISTER:
						if ( verify_session_id( request->session_id ) != 0 ) { break; }
						sentbytes=exec_capi_register((void*)in_packet);
						break;
					case TYPE_CAPI_RELEASE:
						if ( verify_session_id( request->session_id ) != 0 ) { break; }
						sentbytes=exec_capi_release ((void*)in_packet);
						break;
					case TYPE_CAPI_INSTALLED:
						if ( verify_session_id( request->session_id ) != 0 ) { break; }
						sentbytes=exec_capi_isinstalled ((void*)in_packet);
						break;
					case TYPE_CAPI_VERSION:
						if ( verify_session_id( request->session_id ) != 0 ) { break; }
						sentbytes=exec_capi_version((void*)in_packet);
						break;
					case TYPE_CAPI_SERIAL:
						if ( verify_session_id( request->session_id ) != 0 ) { break; }
						sentbytes=exec_capi_serial ((void*)in_packet);
						break;
					case TYPE_CAPI_MANUFACTURER:
						if ( verify_session_id( request->session_id ) != 0 ) { break; }
						sentbytes=exec_capi_manufacturer((void*)in_packet);
						break;
					case TYPE_CAPI_PROFILE:
						if ( verify_session_id( request->session_id ) != 0 ) { break; }
						sentbytes=exec_capi_profile((void*)in_packet);
						break;
					case TYPE_CAPI_WAITFORSIGNAL:
						if ( verify_session_id( request->session_id ) != 0 ) { break; }
						sentbytes=exec_capi_waitforsignal((void*)in_packet);
						break;
					case TYPE_CAPI_GETMESSAGE:
						if ( verify_session_id( request->session_id ) != 0 ) { break; }
						sentbytes=exec_capi_getmessage((void*)in_packet);
						break;
			    		case TYPE_CAPI_PUTMESSAGE:
						if ( verify_session_id( request->session_id ) != 0 ) { break; }
						sentbytes=exec_capi_putmessage((void*)in_packet);
						break;
					default:
						syslog(LOG_NOTICE, "invalid message dispatched.");
				}
				if(sentbytes<1)
				{
					syslog(LOG_WARNING, "answer was not sent: %s",sys_errlist[errno]);
				}
			}
			// test for timeout:
			//now = time ( NULL );
			//if ( now >= init + timeout ) {
			//	xquit ( 1 );
			//}
		}
		////////////////////////////////////////////////////////////////////////////////
	}
exit ( 0 );
}

