CC=gcc
CFLAGS=-I../../c/include -I.
LDIR =../lib
LIBS=

DEPS =
OBJ = idm_monitor.o ../../c/src/isn_dispatch.o ../../c/src/isn_msg.o ../../c/src/posix/isn_udp.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

idm_monitor: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f *.o *~ core
