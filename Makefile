CFLAGS=-O3 -fomit-frame-pointer
ifeq ($(OS),Windows_NT)
    LIBS=-lrt `sdl2-config --libs`
    CFLAGS+=-w 
else
    LIBS=-lrt -lSDL2
endif

all: libpizza.a
	gcc $(CFLAGS) pizza.c -I lib lib/libpizza.a -o emu-pizza $(LIBS)

libpizza.a:
	make -C lib

clean: 
	rm -f *.o
	make -C cpu clean
	make -C system clean
