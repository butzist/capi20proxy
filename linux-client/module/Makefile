all : capiproxy.o

capiproxy.o : capiproxy.c *.h
	gcc -D__KERNEL__ -I/usr/src/linux-2.4.19/include -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fomit-frame-pointer -pipe -mpreferred-stack-boundary=2 -march=athlon    -nostdinc -I /usr/lib/gcc-lib/i386-redhat-linux/2.96/include -DKBUILD_BASENAME=capi20proxy  -DEXPORT_SYMTAB -c capiproxy.c
	ld -r -o capi20proxy.o capiproxy.o

