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
// The main header
//
//////////////////////////////////////////////////////////////////////////////

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

// This one is my favourite: we *need* libcapi20....
#include <capi20.h>

#include <syslog.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "protocol.h"

//#include "config/config.h"
//#include "auth/ipfilter.h"



#define _progname_long "capi20proxy/capifwd daemon"
#define _version "0.6.3"





#define	_PACKET_SIZE 10000

#define APPL_ID(msg)	(*((unsigned int*)(((char*)msg)+2)))
#define CAPI_NCCI(msg)	(*((unsigned long*)(((char*)msg)+8)))
#define CAPI_CMD1(msg)	(*((unsigned char*)(((char*)msg)+4)))
#define CAPI_CMD2(msg)	(*((unsigned char*)(((char*)msg)+5)))





// version things.
#define MY_MAJOR 1
#define MY_MINOR 1




/////////////////
// These are the application indexing functions and data structures
// by Adam. I don't like them, really.
struct appl_list
{
	struct appl_list* next;
	unsigned long ApplID;
};
extern struct appl_list* registered_apps;

void add_app(unsigned long  _ApplID, struct appl_list** _list);
void del_app(unsigned long _ApplID, struct appl_list** _list);
unsigned long getfirst_app(struct appl_list** _list);
void release_all(struct appl_list** _list);
/////////////////



// Set the active controller. Some nice hack by Adam - I don't really
// get his point again.
void SET_CTRL(char* _msg, unsigned char _ctrl);





//////////// NETWORKING VARIABLES //////////////////////////////////////////////////////////////

int port;
int timeout;
char *configfile;


int sc;				                // local socket descriptor
int sock;			                // remote socket descriptor
struct sockaddr_in local_addr;      // local socket properties
struct sockaddr_in remote_addr;     // remote socket properties

char out_packet[_PACKET_SIZE];
int numbytes;




unsigned long sessionID;   // session id (assigned at each fork())
unsigned long auth_types;  // index of supported auth. types




struct __version_t local_version;
struct _client_info {
	int os;							// client operating system
	struct __version_t version;		// client program version
	char name[64];					// client log-name
	int messageBufferLen;			// buffer length used by the client
} client_info;




int quit( int ret );
int xquit ( int ret, char excuse[128]);
int verify_session_id( unsigned id );

void sigchild(int signal);
void sigparent(int signal);
int become_daemon();




//////////////////////////////////////////////////
// all those CAPI functions....
//
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
//////////////////////////////////////////////////


/* command-line evaluation: */
int eval_cmdline(int argc, char* argv[]);
