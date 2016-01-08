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
#include "cpudiag.h"

/* rom memory segment */
uint8_t rom[65536];

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");

    if (f == NULL)
    {
        printf("error: Couldn't open %s\n", argv[1]);
        return 1;
    }

    /* load ROM in memory */
    size_t sz = fread(rom, 1, 65536, f);

    /* check for errors   */
    if (sz < 1)
    {
        printf("error: Cannot read %s\n", argv[1]);
        return 1;
    }

    fclose(f);
    
    /* try to recognize the ROM by size... (forgive me, it's just a beginning) */
    if (sz == 1453)
        cpudiag_start(rom, sz);
    else if (sz == 8192)
        space_invaders_start(rom, sz);
    else
        printf("unknown ROM\n");

    return 0;
}
