SRCS=$(wildcard *.c)
# SRCS=gameboy.c
CFLAGS=-Wall -I.. -c -pthread -O2
CPU_HEADERS=$(wildcard ../cpu/*.h)

all: $(CPU_HEADERS) exercize_z80.o
	gcc $(CFLAGS) $(SRCS) 

clean: 
	rm -f *.o
