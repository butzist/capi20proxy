all : capi20proxy.o

capi20proxy.o : capiproxy.o
	ld -m elf_i386 -r -o capi20proxy.o capiproxy.o

capiproxy.o : capiproxy.c *.h
	gcc -D__KERNEL__ -I/usr/src/linux-2.4.19/include -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fomit-frame-pointer -pipe -mpreferred-stack-boundary=2 -march=athlon  -DMODULE -DMODVERSIONS -include /usr/src/linux-2.4.19/include/linux/modversions.h  -nostdinc -I /usr/lib/gcc-lib/i386-redhat-linux/2.96/include -DKBUILD_BASENAME=capiproxy -DEXPORT_SYMTAB -c -o capiproxy.o capiproxy.c

clean:
	rm -f *.o
