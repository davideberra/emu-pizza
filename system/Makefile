SRCS=$(wildcard *.c)
CFLAGS=-Wall -O3 -I.. -c
CPU_HEADERS=$(wildcard ../cpu/*.h)

all: $(CPU_HEADERS) 
	gcc $(CFLAGS) $(SRCS) 

clean: 
	rm -f *.o
