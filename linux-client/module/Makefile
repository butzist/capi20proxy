all : capi20proxy.o

capi20proxy.o : capi20proxy.o
	ld -m elf_i386 -r -o capi20proxy.o capi20proxy.s

capi20proxy.s : capi20proxy.c *.h
	gcc -DNOTASK -D__KERNEL__ -I/usr/src/linux-2.4.19/include -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fomit-frame-pointer -pipe -mpreferred-stack-boundary=2 -march=i686 -malign-functions=4 -DMODULE -DMODVERSIONS -include /usr/src/linux-2.4.19/include/linux/modversions.h  -nostdinc -I/usr/lib/gcc-lib/i386-linux/2.95.4/include -DKBUILD_BASENAME=capi20proxy -c -o capi20proxy.s capi20proxy.c
clean:
	rm -f *.o
