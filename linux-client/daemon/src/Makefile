all : c2pclient

c2pclient : c2pclient.o unix.o
	gcc -Wall -O2 -o c2pclient *.o

*.o : *.c *.h
	gcc -c -O2 -Wall -o unix.o unix.c
	gcc -c -O2 -Wall -o c2pclient.o c2pclient.c
