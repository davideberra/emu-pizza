GTK_CFLAGS=`pkg-config --cflags gtk+-3.0`
GTK_LIBS=`pkg-config --libs gtk+-3.0`
SRCS=main.c i8080.c cpudiag.c space_invaders.c exercize.c
CFLAGS=-Wall 
LIBS=-lrt -lSDL2

all: emu-pizza

emu-pizza: main.o i8080.o cpudiag.o space_invaders.o exercize.o
	gcc $(CFLAGS) $(SRCS) -o emu-pizza $(LIBS)

clean: 
	rm -f *.o
