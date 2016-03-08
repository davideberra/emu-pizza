# Emu-pizza
An Intel 8080, Zilog Z80 and new born Gameboy emulator.... (still dunno what i want to be)

Requirements
-----------
Emu-pizza requires libSDL2 to compile and run Space Invaders and Gameboy games. To install it

on an APT based distro:
```
sudo apt-get install libsdl2-dev
```

on a YUM based distro:
```
sudo yum install SDL2-devel
```

Compile
-------
```
make
```

Usage 
-----
```
emu-pizza [cpudiag.bin|invaders|8080EX1.com|gameboy rom]
```

Space Invaders keys
-------------------
* Arrow up -- Add 1 credit
* Arrow left/right -- Move ship
* Space -- Fire
* 1/2 -- Start 1/2 player(s)
* Q -- Exit

Gameboy keys
-------------------
* Arrows -- Arrows (rly?)
* Enter -- Start
* Space -- Select
* Z/X -- A/B buttons
* Q -- Exit

Supported ROMS
--------------
* [cpudiag.bin](http://www.emulator101.com/files/cpudiag.bin) -- An Intel 8080/8085 diagnostic ROM [asm file](http://www.emulator101.com/files/cpudiag.asm) 
* [8080 Exercizer](https://github.com/begoon/8080ex1) -- A tougher Intel 8080 exercizer
* Zextest -- A Zilog Z80 exercizer
* Space Invaders -- Guess what
* Gameboy roms with no MBC -- (e.g. Tetris, Dr Mario, Amida)

Credits
-------

Thanks to [Emulator 101](http://www.emulator101.com), the source of all my current knowledge on 8080 emulation
