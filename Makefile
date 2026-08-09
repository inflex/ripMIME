# 
# VERSION CHANGES
#
# 0.1.13: Added -I.

LOCATION=/usr/local
VERSION=0.1.15
# VERSION changes
#---------------------
# 0.1.14: Added 'install' usage
#
#CC=gcc

# if there isn't already a default CFLAGS set,
#	use our recommended settings.
#CFLAGS ?= -Wall -g -O2 -Werror $(CPU_OPTS)
#CFLAGS=-Wall -g -O2 -Wundef -Wshadow -Wsign-compare -I.
CFLAGS=-Wall  -I. -O2 

# OLE decoding is still considered to be 'beta' mode - so it 
#	disabled in the stable release of ripMIME for now
# You can turn it on, but watch out for various office files
#	which may/probably-will break ripMIME with a segfault
#	or other strange errors.  If you do wish to test this
#	and find a dud mailpack, please send through to
#	mailpacks@pldaniels.com
#
COMPONENTS= -DRIPOLE
LIBS= 
#COMPONENTS= 

#  DEBUGGING Related Flags

OBJ=ripmime 
RIPOLE_OBJS= ripOLE/ole.o ripOLE/olestream-unwrap.o ripOLE/bytedecoders.o  ripOLE/bt-int.o
#RIPOLE_OBJS=
OFILES= strstack.o mime.o ffget.o MIME_headers.o tnef/tnef.o rawget.o pldstr.o logger.o libmime-decoders.o boundary-stack.o uuencode.o filename-filters.o $(RIPOLE_OBJS)


default: tnef/tnef.o ripmime ripOLE/ole.o

debug: CFLAGS=-Wall -ggdb -DDEBUG -I. -O0
debug: default

buildcodes.h: 
	./generate-buildcodes.sh

ripOLE/ole.o:
	./build_ripOLE

tnef/tnef.o:
	./build_tnef

.c.o:
	${CC} ${CFLAGS} $(COMPONENTS) -c $*.c

all: ${OBJ} 


solib: ${OFILES} ripmime-api.o
	gcc --shared -Wl,-soname,libripmime.so.1 ${OFILES} ripmime-api.o -o libripmime.so.1.4.0 -lc

libripmime: ${OFILES} ripmime-api.o
	ar ruvs libripmime.a ${OFILES}  ripmime-api.o

ripl: ripmime.a
	${CC} ${CFLAGS} ripmime.c ripmime.a -o ripmime

sco: ${OFILES}
	${CC} ${CFLAGS} ripmime.c ${OFILES} -o ripmime -lsocket

ripmime: ${OFILES} ripmime.c buildcodes.h
	${CC} ${CFLAGS} $(COMPONENTS) ripmime.c ${OFILES} -o ripmime ${LIBS}

riptest: ${OFILES}
	${CC} ${CFLAGS} riptest.c ${OFILES} -o riptest

install: ${OBJ}
	strip ripmime
	install -d ${LOCATION}/bin/
	install -m 755 ripmime ${LOCATION}/bin/
	install -d ${LOCATION}/man/
	install -m 644 ripmime.1  ${LOCATION}/man/man1

ffget_test: ffget_mmap_test.c ffget_mmap.[ch] logger.o ffget_mmap.o
	${CC} ${CFLAGS} ffget_mmap_test.c logger.o ffget_mmap.o -o ffgt

clean:
	rm -f *.o *core ${OBJ} buildcodes.h
	rm -f tnef/*.o
	rm -f ripOLE/*.o ripOLE/ripole

MIMEH: MIME_headers.o strstack.o
	${CC} ${CFLAGS} MIMEH_test.c MIME_headers.o strstack.o -o MIMEH_test

check: @./test.sh
