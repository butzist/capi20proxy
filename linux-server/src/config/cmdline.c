



/* alpha code to evaluate the following command-line parameters: 
 *
 *  -f <filename>: read the following config file instead of /etc/capifwd.conf
 *  -d: debug mode, don't become daemon and send any output to stdout 
 *  -v and -h: print version information and exit
 *  -t <number>: enable server-side timeout of <number> seconds 
 *  -p <number>: listen on tcp/ip port <number>
 *
 */

#include "../capifwd.h"


int eval_cmdline( int argc, char* argv[] ) {
	int i; 
	int d;
	if ( argc == 1 ) {
		return 0;
	}

	for ( i = 1; i < argc; i++) {
			
		if ( !strcmp("-f",argv[i]) || !strcmp("--file", argv[i]) )
		{
			i++;
			//printf("config file is: %s\n", argv[i] );
			configfile = (char*) realloc(configfile, strlen(argv[i]) * sizeof(char)); 
		}
		else if ( !strcmp("-p",argv[i]) || !strcmp("--port",argv[i]) )
		{
			i++;
			d = atoi( argv[i] );
			if ( d == 0 ) {
				//printf("illegal port number\n"); 
			} 
			else {
				//printf("port is %d\n", d);
				port = d;
			}
		}
		else if ( !strcmp("-v",argv[i]) || !strcmp("-h",argv[1]) ||
		     !strcmp("--version", argv[1]) || !strcmp("--help", argv[i]) ) 
		{
			printf("
%s version %s
copyright(c) 2002: f. lindenberg, a. szalkowski
usage:
\t%s [options]\n\n Where options is one of:
-h --help or
-v --version\tprint this info and quit.
-f --file\tdefine the configuration file (makes no 
\t\tsense without config file support).
-p --port\tdefine the port number.
", _progname_long, _version, argv[0]);
			exit ( 0 );
		}
		
			
	}
	return 0;
}



