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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <syslog.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <capi20.h>
#include <linux/capi.h>
#include <linux/kernelcapi.h>

#include "protocol.h"

#define _progname_long "capi20proxy/capifwd daemon"
#define _version "0.6.3"

#define	_PACKET_SIZE 10000

#define APPL_ID(msg)	(*((unsigned int*)(((char*)msg)+2)))
#define CAPI_NCCI(msg)	(*((unsigned long*)(((char*)msg)+8)))
#define CAPI_CMD1(msg)	(*((unsigned char*)(((char*)msg)+4)))
#define CAPI_CMD2(msg)	(*((unsigned char*)(((char*)msg)+5)))

#define MY_MAJOR 1
#define MY_MINOR 2

#define STD_AUTH_REQUIRED 1
#define STD_PORT	6674

int port;
int timeout;
int sc;
int sock;
int numbytes;
int debug;

unsigned long sessionID;
unsigned long auth_types;

char *configfile;
char out_packet[_PACKET_SIZE];

struct sockaddr_in local_addr;
struct sockaddr_in remote_addr;
struct __version_t local_version;


struct _client_info {
	int os;							// client operating system
	struct __version_t version;		// client program version
	char name[64];					// client log-name
	int messageBufferLen;			// buffer length used by the client
} client_info;

struct AUTH_USERPASS_DATA {
	unsigned uname_len;
	unsigned passwd_len;
};

extern char registered_apps[];

void add_app(unsigned long  _ApplID, char* _list);
void del_app(unsigned long _ApplID, char* _list);
unsigned long getfirst_app(char* _list);
void release_all(char* _list);
// Set the active controller. Some nice hack by Adam...
void SET_CTRL(char* _msg, unsigned char _ctrl);

// these are to be depreceated in a future version.
int quit( int ret );
int xquit ( int ret, char excuse[128]);

// signal handlers for SIGTERM, SIGHUP etc.
void sigchild(int signal);
void sigparent(int signal);

// this forks the application into the nackground.
int become_daemon();

// command-line evaluation:
int eval_cmdline(int argc, char* argv[]);

// authentification:
int up_auth(char *indata);
int ip_auth();
int verify_session_id( unsigned id );

// the packet handler functions....
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






