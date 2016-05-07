/*

    This file is part of Emu-Pizza

    Emu-Pizza is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Emu-Pizza is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Emu-Pizza.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include "system/cpudiag.h"
#include "system/gameboy.h"
#include "system/exercize.h"
#include "system/exercize_z80.h"
#include "system/space_invaders.h"

/* rom memory segment */
uint8_t rom[2 << 24];

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");

    if (f == NULL)
    {
        printf("error: Couldn't open %s\n", argv[1]);
        return 1;
    }

    /* load ROM in memory */
    size_t sz = fread(rom, 1, (2 << 24), f);

    /* check for errors   */
    if (sz < 1)
    {
        printf("error: Cannot read %s\n", argv[1]);
        return 1;
    }

    fclose(f);
    
    /* try to recognize the ROM by size.. (forgive me, it's just a beginning) */
    if (sz == 1453)
        cpudiag_start(rom, sz);
    else if (sz == 12288)
        space_invaders_start(rom, sz);
    else if (sz == 8192)
        space_invaders_start(rom, sz);
    else if (sz == 4608)
        exercize_start(rom, sz);
    else if (sz == 1024)
        exercize_start(rom, sz);
    else if (sz == 8585)
        exercize_start(rom, sz);
    else if (sz == 8704)
        exercize_z80_start(rom, sz);
    else
        gameboy_start(argv[1], rom, sz);

    return 0;
}
