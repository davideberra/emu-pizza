SRCS=$(wildcard *.c)
# SRCS=gameboy.c
CFLAGS=-I.. -c -pthread -O3 -Wall
CPU_HEADERS=$(wildcard ../cpu/*.h)

all: $(CPU_HEADERS) exercize_z80.o
	gcc $(CFLAGS) $(SRCS) 

clean: 
	rm -f *.o
