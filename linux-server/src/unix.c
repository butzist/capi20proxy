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
// Some standard UNIX functions ....
//
//////////////////////////////////////////////////////////////////////////////


#include "capifwd.h"



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

// Signal handlers
// for main process
void sigparent(int signal)
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
void sigchild(int signal)
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

// SIGCHLD handler 
//

void sigchld( int signal) 
{
	int pid;
	pid = waitpid(-1, NULL, WNOHANG);
	while (pid > 0) {
		pid = waitpid(-1, NULL, WNOHANG);
	}

}


int become_daemon() {

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

	// override some signals
	signal(SIGINT, sigparent);
	signal(SIGHUP, sigparent);
	signal(SIGTERM, sigparent);
	signal(SIGUSR1, sigparent);
	signal(SIGCHLD, sigchld);
	
  	openlog ("capifwd",LOG_PID, LOG_DAEMON);
  	syslog ( LOG_NOTICE, "capifwd server started.");

	return 0;
}
