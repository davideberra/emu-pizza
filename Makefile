GTK_CFLAGS=`pkg-config --cflags gtk+-3.0`
GTK_LIBS=`pkg-config --libs gtk+-3.0`
CFLAGS=-lpthread -O2 -fomit-frame-pointer
LIBS=-lrt -lSDL2

CPU_OBJS=$(patsubst %.c,%.o,$(wildcard cpu/*.c))
SYSTEM_OBJS=$(patsubst %.c,%.o,$(wildcard system/*.c))

all:
	make -C cpu
	make -C system
	gcc $(CFLAGS) main.c $(CPU_OBJS) $(SYSTEM_OBJS) -o emu-pizza $(LIBS)

clean: 
	rm -f *.o
	make -C cpu clean
	make -C system clean
