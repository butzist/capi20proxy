//////////////////////////////////////////////////////////////////////////////
// CAPI 2.0 Proxy Project:
//   c2pclient CAPI 2.0 Proxy for linux.
// Copyright(C) 2002: F. Lindenberg, A. Szalkowski, B. Gerald.
//
// This program is free software following the terms of the GNU GPL.
// please refer to http://www.gnu.org/licenses/gpl.html .
//
// Support can be obtained from the capi20proxy-public@lists.sourceforge.net
// mailing lists.
//
// For this module, also contact: frlind@frlind.de Please put capi20proxy
// somewhere in your Subject:-Line.
//////////////////////////////////////////////////////////////////////////////
//
// Some standard UNIX functions ....
//
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

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
/*	signal(SIGINT, sigparent);
	signal(SIGHUP, sigparent);
	signal(SIGTERM, sigparent);
	signal(SIGUSR1, sigparent);
	signal(SIGCHLD, sigchld);
*/	
  	openlog ("c2pclient",LOG_PID, LOG_DAEMON);
  	syslog ( LOG_NOTICE, "c2pclient started.");

	return 0;
}
