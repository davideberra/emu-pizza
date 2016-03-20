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
#include <string.h>
#include "cpu/z80.h"
#include "system/cpudiag.h"

/* cpu diag uses z80 CPU, so let's instanciate its state struct */
z80_state_t *z80_state;

/* entry point for cpudiag emulate */
void cpudiag_start(uint8_t *rom, size_t size)
{
    uint16_t addr;

    /* sanity check */
    if ((size + 0x100) > Z80_MAX_MEMORY)
    {
        printf("this ROM can't fit into memory\n");
        return;
    }

    /* init 8080 */
    z80_state = z80_init(); 

    /* load ROM at 0x0100 address of system memory */
    mmu_load(rom, size, 0x100);

//    memcpy(&z80_state->memory[0x100], rom, size);

    /* init PC to 0x0100 */
    z80_state->pc = 0x0100;

    /* running stuff! */
    for (;;)
    {
        uint8_t op = mmu_read(z80_state->pc);

        /* override CALL instruction */
        if (op == 0xCD)
        {
/*            addr = (uint16_t) z80_state->memory[z80_state->pc + 1] + \
                   (uint16_t) (z80_state->memory[z80_state->pc + 2] << 8);
*/
            addr = mmu_read_16(z80_state->pc + 1);

            if (addr == 5)
            {
                if (z80_state->c == 9)
                {
                    uint16_t offset = (z80_state->d<<8) | (z80_state->e);
                    unsigned char *str = mmu_addr(offset + 3);
                    while (*str != '$')
                         printf("%c", *str++);

                    printf("\n");
                }

                break;
            }      
            else if (addr == 0)
                    break;
        }


        if (z80_execute(op))
            break;
    }

    return; 
}
