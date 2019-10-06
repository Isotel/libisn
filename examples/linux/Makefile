CC=gcc
CFLAGS=-I../../c/include -I.
LDIR =../lib
LIBS=

DEPS =
OBJ = idm_monitor.o ../../c/src/isn_dispatch.o ../../c/src/isn_msg.o ../../c/src/posix/isn_udp_driver.o
OBJ_EXAMPLE = udp_example.o ../../c/src/isn_dispatch.o ../../c/src/isn_msg.o ../../c/src/posix/isn_udp_driver.o
OBJ_HOST = udp_host.o ../../c/src/isn_dispatch.o ../../c/src/isn_msg.o ../../c/src/posix/isn_udp_driver.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

idm_monitor: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

udp_example: $(OBJ_EXAMPLE)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

udp_host: $(OBJ_HOST)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f *.o *~ core
