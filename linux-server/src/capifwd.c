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
// This is the main source file that contains whatever didn't fit anywhere
// else.
// main() belongs to this category.
//
//////////////////////////////////////////////////////////////////////////////


#include "capifwd.h"


////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////// MAIN //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

int main ( int argc, char* argv[] ) {
	struct REQUEST_HEADER *request;     // the request header
   	char in_packet[_PACKET_SIZE];
  	char msg[50];
	int remote_size,numbytes,sentbytes;

	struct timeval tv; 
	int rt;
	fd_set accept_w;
	
	auth_types = AUTH_NO_AUTH;
	local_version.major = MY_MAJOR;
	local_version.minor = MY_MINOR;

	become_daemon();


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
  strcpy ( argv[0], "capifwd_listen");
  remote_size = sizeof (struct sockaddr_in);
  sessionID = 0;
  while ( 1 ) {

		FD_ZERO(&accept_w);
		FD_SET(sc, &accept_w);
		rt = 0; 
		while (!rt) {
			tv.tv_sec=1;
			tv.tv_usec=0;
			rt = select( 1, &accept_w, NULL, NULL, &tv); 
			while(waitpid(-1,NULL,WNOHANG)>0);
		}	
			
		sock = accept ( sc, (struct sockaddr*)&remote_addr, &remote_size );

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
			syslog (LOG_WARNING, "forking failed, capi20proxy exits!");
			close ( sc );
			close ( sock );
			exit ( 1 );
		default: continue;
		}

		sprintf(msg, "capifwd_handle %s", inet_ntoa(remote_addr.sin_addr));
		strcpy ( argv[0], msg);
		sprintf(msg, "incoming connection from: %s", inet_ntoa(remote_addr.sin_addr));
		syslog ( LOG_NOTICE, msg );
		//init = time (NULL);
		pthread_mutex_init(&smtx,NULL);

		signal(SIGINT, sigchild);
		signal(SIGHUP, sigchild);
		signal(SIGTERM, sigchild);
		signal(SIGUSR1, sigchild);

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
				//init = time ( NULL );
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
