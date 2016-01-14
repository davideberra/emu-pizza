# Emu-pizza
Another Intel 8080 emulator.... so far...

Requirements
-----------
Emu-pizza requires libSDL2 to compile and run Space Invaders. To install it

on an APT based distro:
```
sudo apt-get install libsdl2-dev
```

on a YUM based distro:
```
sudo SDL2-devel
```

Compile
-------
```
make
```

Usage 
-----
```
emu-pizza [cpudiag.bin|invaders|8080EX1.com]
```

Supported ROMS
--------------
* [cpudiag.bin](http://www.emulator101.com/files/cpudiag.bin) -- An Intel 8080/8085 diagnostic ROM [asm file](http://www.emulator101.com/files/cpudiag.asm) 
* [8080 Exercizer](https://github.com/begoon/8080ex1) -- A tougher Intel 8080 exercizer
* Space Invaders -- Guess what

Credits
-------

Thanks to [Emulator 101](http://www.emulator101.com), the source of all my current knowledge on 8080 emulation
