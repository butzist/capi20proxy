########################################################################
#
# Makefile Rev. 2.0 for capifwd 0.6.3 
# (C) 2002: F. Lindenberg, GPLd.
#
########################################################################




all:

#core code:
	gcc -c capifwd.c $(CFLAGS)
	gcc -c unix.c $(CFLAGS)
	gcc -c appindex.c $(CFLAGS)

#auth code:
	gcc -c auth/auth.c $(CFLAGS)
	
#config code: 
	gcc -c config/cmdline.c $(CFLAGS)
	
#capi commands:
	gcc -c capi/etc.c $(CFLAGS)
	gcc -c capi/isinstalled.c $(CFLAGS)
	gcc -c capi/profile.c $(CFLAGS)
	gcc -c capi/getmessage.c $(CFLAGS)
	gcc -c capi/putmessage.c $(CFLAGS)
	gcc -c capi/manufacturer.c $(CFLAGS)
	gcc -c capi/waitforsignal.c $(CFLAGS)
	gcc -c capi/version.c $(CFLAGS)
	gcc -c capi/serial.c $(CFLAGS)
	gcc -c capi/register.c $(CFLAGS)
	gcc -c capi/release.c $(CFLAGS) 
	gcc -c capi/proxy.c $(CFLAGS)

#finished object code generation...
#link:
	gcc -o capifwd *.o -lcapi20 $(CFLAGS)

install: 

	cp capifwd /usr/sbin 
	chmod 774 /usr/sbin/capifwd 
	cp scripts/capifwd /etc/init.d
	chmod 774 /etc/init.d/capifwd 

clean:
	rm -f *.o capifwd
	 


