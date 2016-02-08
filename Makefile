GTK_CFLAGS=`pkg-config --cflags gtk+-3.0`
GTK_LIBS=`pkg-config --libs gtk+-3.0`
CFLAGS=-Wall -O3
LIBS=-lrt -lSDL2

CPU_OBJS=$(wildcard cpu/*.o)
CPU_HEADERS=$(wildcard cpu/*.h)
SYSTEM_OBJS=$(wildcard system/*.o)

all: emu-pizza

$(CPU_OBJS): $(wildcard cpu/*.c) 
	make -C cpu

$(SYSTEM_OBJS): $(wildcard system/*.c) $(wildcard cpu/*.h)
	make -C system

emu-pizza: $(wildcard system/*.c) $(wildcard cpu/*)
	make -C cpu
	make -C system
	gcc $(CFLAGS) main.c $(wildcard system/*.o) $(wildcard cpu/*.o) -o emu-pizza $(LIBS)

clean: 
	rm -f *.o
	make -C cpu clean
	make -C system clean
