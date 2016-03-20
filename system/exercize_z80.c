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
#include <inttypes.h>
#include <time.h>
#include "cpu/z80.h"
#include "exercize_z80.h"

/* this exercizer uses Z80 CPU, so let's instanciate its state struct */
z80_state_t *z80_state;

/* entry point for cpudiag emulate */
void exercize_z80_start(uint8_t *rom, size_t size)
{
    uint8_t  op;

    /* sanity check */
    if (size + 0x0100 > Z80_MAX_MEMORY)
    {
        printf("this ROM can't fit into memory\n");
        return;
    }

    /* init Z80 */
    z80_state = z80_init(); 

    /* load ROM at 0x0100 address of system memory */
    mmu_load(rom, size, 0x0100);

//    memcpy(&z80_state->memory[0x0100], rom, size);

    /* init PC to 0x0100 */
    z80_state->pc = 0x0100;

    /* OUT */
    mmu_write(0, 0xD3);
    mmu_write(1, 0x00);

//    z80_state->memory[0] = 0xd3;
//    z80_state->memory[1] = 0x00;

    /* put a RET on 0x0005 address */
    mmu_write(5, 0xDB);
    mmu_write(6, 0x00);
    mmu_write(7, 0xC9);

//    z80_state->memory[5] = 0xDB; 
//    z80_state->memory[6] = 0x00; 
//    z80_state->memory[7] = 0xC9; 

    uint64_t cyc = 0;

    /* running stuff! */
    for (;;)
    {
        /* get op */
        op = mmu_read(state.pc);

/*        if (cyc >= 5625085075 && cyc <= 5628085075)
            printf("PC:%04x SP:%04x OP:%02x:%02x:%02x:%02x - A:%02x B:%02x C:%02x D:%02x E:%02x H:%02x L:%02x IX:%04x IY:%04x F:%02x MEM:%02x\n", state.pc, state.sp, op, mmu_read(state.pc), mmu_read(state.pc+1), mmu_read(state.pc+2), 
            state.a, state.c, state.c, state.d, state.e, state.h, state.l, *state.ix, *state.iy, *state.f, 
            mmu_read(*state.hl));
*/

        /* override CALL instruction */
        if (op == 0xCD)
        {
            if (mmu_read(z80_state->pc + 2) == 0)
            {
                if (mmu_read(z80_state->pc + 1) == 5)
                {
                    if (z80_state->c == 9)
                    {
                        uint16_t offset = (z80_state->d<<8) | (z80_state->e);
                        unsigned char *str = mmu_addr(offset);
   
                        while (*str != '$')
                             printf("%c", *str++); 
                    }
                    if (z80_state->c == 2) putchar((char) z80_state->e);
                }      
                else if (mmu_read(z80_state->pc + 1) == 0)
                         break;
            }
        }

        if (z80_execute(op))
            break;

        // cyc += state.t;
        cyc++;


        if (z80_state->pc == 0x0000)
        {
            printf("PC = 0x0000 - EXIT\n");
            break;
        }
    }

    printf("T-STATES PASSATI: %"  PRIu64 "\n", state.cycles);

    return; 
}
