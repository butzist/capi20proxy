#
# simple Makefile for CapiFwd Linux-Server. 
# 
# a bit strange. 
#
# (C) 2002: The usual suspectives. 
#

release: 
	gcc -o capifwd -Wall -O99 -lcapi20 -lpthread \
		capifwd.c appindex.c capi/etc.c auth/auth.c \
		capi/proxy.c capi/register.c capi/release.c \
		capi/getmessage.c capi/putmessage.c \
		capi/version.c capi/profile.c capi/isinstalled.c \
		capi/waitforsignal.c capi/serial.c \
		capi/manufacturer.c unix.c

all:
	release

install:
	cp capifwd /usr/sbin && 
	chmod 774 /usr/bin/capifwd &&
	cp scripts/capifwd /etc/init.d/capifwd &&
	chmod 777 /etc/init.d/capifwd

clean: 
	rm -f capifwd 


