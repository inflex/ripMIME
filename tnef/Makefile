

# Possible platforms are SUN, HPUX, DEC, SGI, AIX, Linux and MSDOS
# The main definitions are for 32/16 bits and for byte order, the 
# default is big endian.  You can do -D__TNEF_BYTE_ORDER 1234 for little
# endian
PLATFORM=-D___TNEF_BYTE_ORDER=4321 
CFLAGS=-Wall -g -O2 -I.

default: config.h tnef.h tnef.c
	$(CC) $(CFLAGS) $(PLATFORM) -c tnef.c
	
lib: tnef.o
	ar ruvs tnef.a tnef.o

clean:
	rm -f *.o *.~[ch]
