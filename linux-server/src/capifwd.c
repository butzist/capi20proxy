/* The Capi 2.0 Proxy Linux Server.
 * Copyright (C) 2002: Friedrich Lindenberg, Adam Szalkowski.
 * Contact: capi20proxy-public@lists.sourceforge.net
 * See ../COPYRIGHT for Licensing and Copyright.
 */

#include "capifwd.h"

ssize_t recvn(int fd, void* buf, size_t size)
{
	size_t left = size;
	while (left) {
		ssize_t n = recv(fd, buf, left, 0);
		if (n <= 0)
			return n;
		(char*)buf += n;
		left -= n;
	}
	return size;
}

int main ( int argc, char* argv[] ) {
	struct REQUEST_HEADER *request;     // the request header
   	char in_packet[_PACKET_SIZE];
  	char msg[50];
	int remote_size,numbytes,sentbytes;

	auth_types = AUTH_BY_IP;
	local_version.major = MY_MAJOR;
	local_version.minor = MY_MINOR;

	//set some defaults...  
	port = STD_PORT;
	
	eval_cmdline(argc, argv);
	
	if(!debug) {
		become_daemon();
	}

	memset(registered_apps,0,CAPI_MAXAPPL+1);

	if ( ( sc = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
   		syslog ( LOG_WARNING, sys_errlist[errno] );
   		exit ( 1 );
  	}

  	local_addr.sin_family = AF_INET;
  	local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  	local_addr.sin_port = htons ( port );

  	if ( bind ( sc, (struct sockaddr*)&local_addr, sizeof(local_addr)) == -1 ) {
   		syslog ( LOG_WARNING, sys_errlist[errno] );
   		close ( sc );
   		exit ( 1 );
  	}

  	if ( listen( sc, 3) == -1 ) {
		syslog ( LOG_WARNING, sys_errlist[errno]);
  		close (sc );
		exit ( 1 );
  	}

  	// rename the process: strange code, isn't it?
  	strcpy ( argv[0], "capifwd_wait");
  
  	remote_size = sizeof (struct sockaddr_in);
  	sessionID = 2;
  	while ( 1 ) {
		sock = accept ( sc, (struct sockaddr*)&remote_addr, &remote_size );

		if ( sock == -1 ) {
			syslog(LOG_WARNING, sys_errlist[errno]);
			close(sc);
			exit(1);
		}
		sessionID++;

		if(!debug) {
			switch(fork())
			{
				case 0: break;
				case -1:
					syslog (LOG_WARNING, "Exiting due to fork error");
					close ( sc );
					close ( sock );
					exit ( 1 );
				default: continue;
			}
		}		

		sprintf(msg, "capifwd_handle (%s)", inet_ntoa(remote_addr.sin_addr));
		strcpy ( argv[0], msg);
		sprintf(msg, "Client from %s: spawned new handler %d.", inet_ntoa(remote_addr.sin_addr),getpid());
		syslog ( LOG_NOTICE, msg );

		
		//init = time (NULL);


		signal(SIGINT, sigchild);
		signal(SIGHUP, sigchild);
		signal(SIGTERM, sigchild);
		signal(SIGUSR1, sigchild);

		////////////////////////////////////////////////////////////////////////////////
		// Handling
		while ( 1 ) {
			request = (struct REQUEST_HEADER*) in_packet;
			numbytes = recvn( sock, request, sizeof(*request));

			if(numbytes <= 0) {
				close (sock);
				release_all(registered_apps);	//release the registered applications
				if(numbytes < 0) {
					syslog(LOG_NOTICE, sys_errlist[errno]);
				}
				
				exit ( 0 );
			}
			
			numbytes = recvn(sock, in_packet + sizeof(*request), request->message_len - sizeof(*request)); // read remainder of message
			
			if(numbytes <= 0) {
				close (sock);
				release_all(registered_apps);	//release the registered applications
				if(numbytes < 0) {
					syslog(LOG_NOTICE, sys_errlist[errno]);
				}
				
				exit ( 0 );
			}
			
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
					if ( request->session_id != sessionID ) break;
					sentbytes=exec_capi_register((void*)in_packet);
					break;
				case TYPE_CAPI_RELEASE:
					if (  request->session_id != sessionID ) break;
					sentbytes=exec_capi_release ((void*)in_packet);
					break;
				case TYPE_CAPI_INSTALLED:
					if (  request->session_id != sessionID ) break;
					sentbytes=exec_capi_isinstalled ((void*)in_packet);
					break;
				case TYPE_CAPI_VERSION:
					if (  request->session_id != sessionID ) break;
					sentbytes=exec_capi_version((void*)in_packet);
					break;
				case TYPE_CAPI_SERIAL:
					if (  request->session_id != sessionID ) break;
					sentbytes=exec_capi_serial ((void*)in_packet);
					break;
				case TYPE_CAPI_MANUFACTURER:
					if (  request->session_id != sessionID  ) break;
					sentbytes=exec_capi_manufacturer((void*)in_packet);
					break;
				case TYPE_CAPI_PROFILE:
					if (  request->session_id != sessionID ) break;
					sentbytes=exec_capi_profile((void*)in_packet);
					break;
				case TYPE_CAPI_WAITFORSIGNAL:
					if (  request->session_id != sessionID ) break;
					sentbytes=exec_capi_waitforsignal((void*)in_packet);
					break;
				case TYPE_CAPI_GETMESSAGE:
					if (  request->session_id != sessionID ) break;
					sentbytes=exec_capi_getmessage((void*)in_packet);
					break;
				case TYPE_CAPI_PUTMESSAGE:
					if (  request->session_id != sessionID ) break;
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
		////////////////////////////////////////////////////////////////////////////////
	}
}

