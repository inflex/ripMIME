# 
# VERSION CHANGES
#
# 0.1.13: Added -I.

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
CFLAGS ?= -Wall -Wextra -O2

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

${RIPOLE_OBJS} tnef/tnef.o:
	${MAKE} -C ${@D} ${@F} CFLAGS="${CFLAGS} ${CFLAGS-y}"

.c.o:
	${CC} ${CFLAGS-y} ${COMPONENTS} -c $*.c

CFLAGS-y   = ${CFLAGS}
LDFLAGS-y  = ${LDFLAGS}

all: ${OBJ} 
solib          := libripmime.so.1.4.0
solib-soname   != awk -vlib=${solib} -vsuffix="\\.so\\.[0-9]+" \
	'BEGIN{match(lib,suffix);print substr(lib,1,RSTART+RLENGTH-1)}'
solib-basename != awk -vlib=${solib} -vsuffix="\\.so" \
	'BEGIN{match(lib,suffix);print substr(lib,1,RSTART+RLENGTH-1)}'

${solib}-LDFLAGS  := -shared -Wl,-soname,${solib-soname}
${solib}-objs     := ${OFILES} ripmime-api.o

solib: ${solib}
${solib}: ${${solib}-objs}
	${CC} ${$@-LDFLAGS} ${LDFLAGS-y} ${CFLAGS-y} -o $@ ${$@-objs} ${LIBS}


libripmime: ${OFILES} ripmime-api.o
	ar ruvs libripmime.a ${OFILES}  ripmime-api.o

ripl: ripmime.a
	${CC} ${CFLAGS} ripmime.c ripmime.a -o ripmime

sco: ${OFILES}
	${CC} ${CFLAGS} ripmime.c ${OFILES} -o ripmime -lsocket

ripmime.o: buildcodes.h
ripmime: ${OFILES} ripmime.o
	${CC} ${LDFLAGS-y} ${CFLAGS-y} ${COMPONENTS} $@.o -o $@ ${OFILES} ${LIBS}

riptest: ${OFILES}
	${CC} ${LDFLAGS-y} ${CFLAGS-y} $@.c ${OFILES} -o $@

DESTDIR =
prefix  = /usr/local
  exec_prefix = ${prefix}
    bindir = ${exec_prefix}/bin
    libdir = ${exec_prefix}/lib
  datarootdir = ${prefix}/share
    mandir = ${datarootdir}/man

.PHONY: install install-exec install-man
.PHONY: install-static install-shared install-headers

install-static: libripmime.a
	install -m444 -Dt ${DESTDIR}${libdir} libripmime.a

install-shared: ${solib}
	install -m555 -Dt ${DESTDIR}${libdir} ${solib}
	ln -sf ${solib} ${DESTDIR}${libdir}/${solib-soname}
	ln -sf ${solib} ${DESTDIR}${libdir}/${solib-basename}

install: install-exec install-man
install-exec: ${OBJ}
	install -m555 -s -Dt ${DESTDIR}${bindir} ${OBJ}
install-man:
	install -m444 -Dt ${DESTDIR}${mandir}/man1 ripmime.1

ffget_test: ffget_mmap_test.c ffget_mmap.[ch] logger.o ffget_mmap.o
	${CC} ${LDFLAGS-y} ${CFLAGS-y} ffget_mmap_test.c logger.o ffget_mmap.o -o ffgt

clean:
	rm -f *.o *core ${OBJ} buildcodes.h
	rm -f tnef/*.o
	rm -f ripOLE/*.o ripOLE/ripole

MIMEH: MIME_headers.o strlower.o
	${CC} ${CFLAGS} MIMEH_test.c MIME_headers.o strlower.o -o MIMEH_test
