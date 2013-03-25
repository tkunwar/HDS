#
# Makefile for acp
CC=gcc

#The below line is for debugging
#CFLAGS=-I. -ggdb -Wall -D_FILE_OFFSET_BITS=64
# when linking with libconfig in shared mode
# use this
# CFLAGS=-Wall -g -lcdk -lncurses -lconfig
# when using libconfig in static linking mode
# use this
CFLAGS=-Wall -g -lcdk -lncurses -lpthread -lrt
LIBS=libconfig.a

all:hds
hds: hds.o hds_ui.o hds_common.o hds_config.o hds_core.o
	$(CC) -o $@ $^ $(LIBS) $(CFLAGS)

%.o: %.c
	$(CC) -c $*.c $(CFLAGS)
docs:
	doxygen hds.doxyfile
clean:
	rm -f *.o *.out hds *.log
	rm -r -f doxygen-output
