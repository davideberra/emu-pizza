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
#include <stdint.h>
#include "cpu/z80.h"
#include "exercize.h"

/* cpu diag uses z80 CPU, so let's instanciate its state struct */
z80_state_t *z80_state;

/* entry point for cpudiag emulate */
void exercize_start(uint8_t *rom, size_t size)
{
    unsigned int addr;

    /* sanity check */
    if (size + 0x0100 > Z80_MAX_MEMORY)
    {
        printf("this ROM can't fit into memory\n");
        return;
    }

    /* init 8080 */
    z80_state = z80_init(); 
   
    /* empty memory */
//     bzero(z80_state->memory, 65536);

    /* load ROM at 0x0100 address of system memory */
    mmu_load(rom, size, 0x100);

    // memcpy(&z80_state->memory[0x0100], rom, size);

    /* init PC to 0x0100 */
    z80_state->pc = 0x0100;

    /* put a RET on 0x0005 address */
    mmu_write(5, 0xC9); 

    /* running stuff! */
    for (;;)
    {
        uint8_t op = mmu_read(z80_state->pc);

        /* override CALL instruction */
        if (op == 0xCD)
        {
            addr = mmu_read_16(z80_state->pc + 1);

            if (addr == 5)
            {
                if (z80_state->c == 9)
                {
                    uint16_t offset = (z80_state->d<<8) | (z80_state->e);
                    unsigned char *str = mmu_addr(offset);

                    printf("TWITTONE\n");

                    while (*str != '$')
                         printf("%c", *str++); 
                }
                if (z80_state->c == 2) putchar((char) z80_state->e);
            }      
            else if (addr == 0)
                 {
                     printf("ADDR 0\n");
                     break;
                 }
        }

//        printf("EXECUTO: %02x - STACK: %x\n", op, z80_state->sp);

        if (z80_execute(op))
            break;

        if (z80_state->pc == 0x0000)
        {
            printf("PC = 0x0000 - EXIT\n");
            break;
        }
    }

    return; 
}
