
all: emu-pizza

emu-pizza: main.o i8080.o cpudiag.o space_invaders.o
	gcc main.c i8080.c cpudiag.c space_invaders.c -o emu-pizza -lrt
