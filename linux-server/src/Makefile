########################################################################
#
# Makefile Rev. 2.0 for capifwd 0.6.3 
# (C) 2002: F. Lindenberg, GPLd.
#
########################################################################




all:

#core code:
	gcc -c capifwd.c -Wall -O3
	gcc -c unix.c -Wall -O3
	gcc -c appindex.c -Wall -O3

#auth code:
	gcc -c auth/auth.c -Wall -O3
	
#config code: 
	gcc -c config/cmdline.c -Wall -O3
	
#capi commands:
	gcc -c capi/etc.c -Wall -O3
	gcc -c capi/isinstalled.c -Wall -O3
	gcc -c capi/profile.c -Wall -O3
	gcc -c capi/getmessage.c -Wall -O3
	gcc -c capi/putmessage.c -Wall -O3
	gcc -c capi/manufacturer.c -Wall -O3
	gcc -c capi/waitforsignal.c -Wall -O3
	gcc -c capi/version.c -Wall -O3
	gcc -c capi/serial.c -Wall -O3
	gcc -c capi/register.c -Wall -O3
	gcc -c capi/release.c -Wall -O3 
	gcc -c capi/proxy.c -Wall -O3

#finished object code generation...
#link:
	gcc -o capifwd *.o -Wall -lcapi20 -O3 

install: 

	cp capifwd /usr/sbin 
	chmod 774 /usr/sbin/capifwd 
	cp scripts/capifwd /etc/init.d
	chmod 774 /etc/init.d/capifwd 

clean:
	rm -rf *.o
	 


