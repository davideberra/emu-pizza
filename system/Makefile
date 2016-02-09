SRCS=$(wildcard *.c)
CFLAGS=-Wall -O3 -I.. -c
CPU_HEADERS=$(wildcard ../cpu/*.h)

all: $(CPU_HEADERS) exercize_z80.o
	gcc $(CFLAGS) $(SRCS) 

clean: 
	rm -f *.o
