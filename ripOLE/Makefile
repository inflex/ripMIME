
OBJS= ole.o olestream-unwrap.o bytedecoders.o logger.o pldstr.o bt-int.o mime_element.o
CFLAGS=-Wall -g -O2 -I. 

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) -c $*.c

default: ripole

clean:
	rm -f *.o ripole

ripole: $(OBJS) ripole.[ch]
		$(CC) $(CFLAGS) $(OBJS) $(DEFINES) ripole.c -o ripole
	
validate: ripole
		cp ripole validate
