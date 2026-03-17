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

all: ripmime
debug: CFLAGS=-Wall -ggdb -DDEBUG -I. -O0
debug: all
sco: LIBS += -lsocket
sco: ripmime
.PHONY: all debug sco

buildcodes.h: 
	./generate-buildcodes.sh

clean::
	rm -f buildcodes.h

${RIPOLE_OBJS} tnef/tnef.o:
	${MAKE} -C ${@D} ${@F} CFLAGS="${CFLAGS} ${CFLAGS-y}"

.c.o:
	${CC} ${CFLAGS-y} ${COMPONENTS} -c $*.c

CFLAGS-y   = ${CFLAGS}
LDFLAGS-y  = ${LDFLAGS}
LDFLAGS-y += -Wl,-rpath,${libdir}

CFLAGS-${use-solib}  += -fPIC

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

clean::
	rm -f ${solib}

ARFLAGS = ruvs
libripmime.a: ${OFILES} ripmime-api.o
	${AR} ${ARFLAGS} $@ ${OFILES} ripmime-api.o

clean::
	rm -f libripmime.a

use-solib = n
-lib := ${use-solib:y=${solib}} ${use-solib:n=libripmime.a}
-lib := ${-lib:y=}
-lib := ${-lib:n=}

ripmime.o: buildcodes.h
ripmime: ${-lib} ripmime.o buildcodes.h
	${CC} ${LDFLAGS-y} ${CFLAGS-y} ${COMPONENTS} $@.o -o $@ ${-lib} ${LIBS}

riptest: ${OFILES}
	${CC} ${LDFLAGS-y} ${CFLAGS-y} $@.c ${OFILES} -o $@

clean::
	rm -f ripmime riptest

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

.PHONY: clean
clean::
	rm -f *.o *core
	rm -f tnef/*.o
	rm -f ripOLE/*.o ripOLE/ripole

.PHONY: MIMEH
MIMEH: MIMEH_test
MIMEH_test-objs = MIMEH_test.o MIME_headers.o strlower.o
MIMEH_test: ${MIMEH_test-objs}
	${CC} ${LDFLAGS-y} ${CFLAGS-y} ${$@-objs} -o $@
clean::
	rm -f MIMEH_test

# Macro to extract a quoted & unexpanded variable
quote = '$(subst ','\'',$(value $1))'

# Save and restore configuration variables
-include .config
_ != { \
	echo 'AR      = '${AR:Q}$(call quote,AR); \
	echo 'ARFLAGS = '${ARFLAGS:Q}$(call quote,ARFLAGS); \
	echo 'CC      = '${CC:Q}$(call quote,CC); \
	echo 'CFLAGS  = '${CFLAGS:Q}$(call quote,CFLAGS); \
	echo 'LDFLAGS = '${LDFLAGS:Q}$(call quote,LDFLAGS); \
	echo 'LIBS    = '${LIBS:Q}$(call quote,LIBS); \
	echo; \
	echo 'prefix = '${prefix:Q}$(call quote,prefix); \
	echo '  exec_prefix = '${exec_prefix:Q}$(call quote,exec_prefix); \
	echo '    bindir = '${bindir:Q}$(call quote,bindir); \
	echo '    libdir = '${libdir:Q}$(call quote,libdir); \
	echo '  datarootdir = '${datarootdir:Q}$(call quote,datarootdir); \
	echo '    mandir = '${mandir:Q}$(call quote,mandir); \
	echo; \
	echo 'use-solib     = '${use-solib:Q}$(call quote,use-solib); \
	} > .config
include .config

# FIXME: variables (unintuitivelly) are expanded when saving in bmake

.PHONY: distclean
distclean:: clean
	rm -f .config
