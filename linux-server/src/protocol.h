/*

  CAPI 2.0 Proxy
  Protocol Definition Release 2 Prototype 4

  7th March 2002. Written in Rannoch School
  8th March 2002. Reviewed in Baden-Baden :-)

  This protocol was designed to provide easy CAPI forwarding over a
  local area network or the internet.
  The protocol is pretty self-explanationary, so no further Docu. is
  provided at the moment. If you have problems, just contact one of
  the maintainers.

*/

/* The CVS log:
 * $Log$
 * Revision 1.2  2002/04/10 12:09:13  butzist
 * looked through the code, now waiting for Fritzle to find the syntax errors and compile it
 * it could work now but many features are missing
 *
 * Revision 1.6  2002/03/29 07:52:06  butzist
 * seems to work (got problems with DATA_B3)
 *
 * Revision 1.5  2002/03/21 15:16:42  butzist
 * started rewriting for new protocol
 *
 * Revision 1.3  2002/03/09 16:58:52  frlind
 *
 * the new tcp/ip protocol-based protocol revision 2
 *
 */


/* Exlanations of data types used in this ("platform independent") protocol:
 name   		| type 	| size 	| signed 	| byte order
 -------------------------------------------------------------------
 char   		| integer 	| 1 octet 	| yes  	| -
 int    		| integer 	| 2 " 	| yes  	| iA-32 standard
 unsigned  	| integer 	| 2 " 	| no  	|   "
 unsigned long | integer 	| 4 " 	| no  	|   "
*/

#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__
#include <time.h>

// Operating System types

#define OS_TYPE_WINDOWS	1
#define OS_TYPE_LINUX	2
#define OS_TYPE_GENERIC	0


// Proxy error/warning codes

#define PERROR_NONE		0x00

#define PERROR_AUTH_REQUIRED	0x01	// indicate that you should use authentification (only for use in ANSWER_PROXY_HELO)
#define PERROR_WRONG_SESSION	0x02	// unknown session id, message lost
#define	PERROR_ACCESS_DENIED	0x03	// message not processed, because of lack of access rights
#define PERROR_UNKNOWN_APP		0x04	// unknown/illegal ApplId in the message
#define PERROR_UNKNOWN_CTRL		0x05	// unknown/illegal ControllerId in the message
#define PERROR_PARSE_ERROR		0x06	// error in message structure
#define PERROR_AUTH_TYPE_NOT_SUPPORTED	0x07;
#define PERROR_INCOMPATIBLE_VERSION	0x08;

#define PERROR_NO_RESSOURCES			0x80	// necessary resources not available
#define PERROR_CRITICAL_INTERNAL_ERROR	0x81	// server must shut down
#define PERROR_SOCKET_ERROR				0x82	// maybe try to resend the message
#define PERROR_CRITICAL_SOCKET_ERROR	0x83	// must disconnect connection

#define PERROR_CAPI_ERROR	0xff	// message not processed due to CAPI error

// auth types

#define AUTH_NO_AUTH	0x0000
#define AUTH_BY_IP		0x0001	// auth
#define	AUTH_USERPASS	0x0002
#define	AUTH_RSA		0x0100	// encryption for get/put message
#define	AUTH_BLOWFISH	0x0200

/// define types:

#define TYPE_CAPI_REGISTER		1
#define TYPE_CAPI_RELEASE		2
#define TYPE_CAPI_PUTMESSAGE	3
#define TYPE_CAPI_GETMESSAGE	4
#define TYPE_CAPI_WAITFORSIGNAL	5
#define TYPE_CAPI_MANUFACTURER	6
#define TYPE_CAPI_VERSION		7
#define TYPE_CAPI_SERIAL		8
#define TYPE_CAPI_PROFILE		9
#define TYPE_CAPI_INSTALLED		10
#define TYPE_PROXY_HELO			99
#define TYPE_PROXY_AUTH			98
#define TYPE_PROXY_KEEPALIVE	97
#define TYPE_PROXY_SHUTDOWN		96


//const char *revision="$Revision$";

struct __version_t {
 unsigned long major;  // major version for incompatible versions
 unsigned long minor;  // minor version for compatible versions
};

// CLIENT REQUESTS //
// protocol specific
struct REQUEST_PROXY_HELO {  // type number: 99
 char name[64];    // name of the client (for logging)
 int os;      // operating system of the client
 struct __version_t version; // version of the client
};

struct REQUEST_PROXY_AUTH {  // type number: 98
 unsigned auth_len;
};

struct REQUEST_PROXY_KEEPALIVE {  // type number: 97
};

// CAPI specific
struct REQUEST_CAPI_REGISTER {  // type number: 1
	unsigned long messageBufferSize;
	unsigned long maxLogicalConnection;
	unsigned long maxBDataBlocks;
	unsigned long maxBDataLen;	// added again
};

struct REQUEST_CAPI_RELEASE {  // type number: 2
};

struct REQUEST_CAPI_PUTMESSAGE {  // type number: 3
 // length of message provided in the header as data_len
};

struct REQUEST_CAPI_GETMESSAGE {  // type number: 4
};

struct REQUEST_CAPI_WAITFORSIGNAL { // type number: 5
 // we do not provide a timeout value, for the Win32 CAPI does not support
};

struct REQUEST_CAPI_MANUFACTURER {  // type number: 6
};

struct REQUEST_CAPI_VERSION {  // type number: 7
};

struct REQUEST_CAPI_SERIAL {  // type number: 8
};


struct REQUEST_CAPI_PROFILE {  // type number: 9
};

struct REQUEST_CAPI_INSTALLED {  // type number: 10
};


// SERVER ANSWERS //

// protocol specific
struct ANSWER_PROXY_HELO {  // type number: 99
 char name[64];    // some kind of name for the server (zero-terminated)
 int os;      // the operating system of the server
 struct __version_t version; // the version of the server
 unsigned long auth_type;  // the server tells the client which auth-methods it supports (each bit represents one method) !changed!
 int timeout;			// in seconds, -1 means no timeout
};

struct ANSWER_PROXY_AUTH {  // type number: 98
 unsigned long auth_type;   // authentication type desired
 unsigned auth_len;   // length of authentication data
};

struct ANSWER_PROXY_KEEPALIVE { // type number: 97
};

struct ANSWER_PROXY_SHUTDOWN {  // type number: 96
 char reason[128];
 // z.B. "Ich muss dringend aufs Klo!"; :-)
 // no answer from the client expected
};

// CAPI specific
struct ANSWER_CAPI_REGISTER {  // type number: 1
};

struct ANSWER_CAPI_RELEASE {  // type number: 2
};

struct ANSWER_CAPI_PUTMESSAGE { // type number: 3
};

struct ANSWER_CAPI_GETMESSAGE { // type number: 4
 // we use the header data_len value to get the length of the message
};

struct ANSWER_CAPI_WAITFORSIGNAL { // type number: 5
};

struct ANSWER_CAPI_MANUFACTURER { // type number: 6
 char manufacturer[64];
};

struct ANSWER_CAPI_VERSION {  // type number: 7
 struct __version_t manufacturer;
 struct __version_t driver;
};

struct ANSWER_CAPI_SERIAL {  // type number: 8
 char serial[8];
};

struct ANSWER_CAPI_PROFILE {  // type number: 9
 char profile[64];
};

struct ANSWER_CAPI_INSTALLED { // type number: 10
};


struct REQUEST_HEADER {
 unsigned message_len;
 unsigned header_len;
 unsigned body_len;
 unsigned data_len;

 unsigned  message_id;
 unsigned  message_type;
 unsigned  long app_id;		// must be long!
 unsigned  controller_id;
 unsigned  session_id;
};


struct ANSWER_HEADER {
 unsigned message_len;
 unsigned header_len;
 unsigned body_len;
 unsigned data_len;

 unsigned  message_id;
 unsigned  message_type;
 unsigned  long app_id;		// must be long!
 unsigned  session_id;
 unsigned  proxy_error;
 unsigned long capi_error;
};
#endif

