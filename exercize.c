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
#include "i8080.h"
#include "exercize.h"

/* cpu diag uses i8080 CPU, so let's instanciate its state struct */
i8080_state_t *i8080_state;

/* helper macros */
#define BC (uint16_t) (((uint16_t) i8080_state->b << 8) + i8080_state->c)
#define DE (uint16_t) (((uint16_t) i8080_state->d << 8) + i8080_state->e)
#define HL (uint16_t) (((uint16_t) i8080_state->h << 8) + i8080_state->l)


/* entry point for cpudiag emulate */
void exercize_start(uint8_t *rom, size_t size)
{
    uint16_t addr;

    /* sanity check */
    if (size + 0x0100 > I8080_MAX_MEMORY)
    {
        printf("this ROM can't fit into memory\n");
        return;
    }

    /* init 8080 */
    i8080_state = i8080_init(); 
    
    /* load ROM at 0x0100 address of system memory */
    memcpy(&i8080_state->memory[0x0100], rom, size);

    /* init PC to 0x0100 */
    i8080_state->pc = 0x0100;

    /* put a RET on 0x0005 address */
    i8080_state->memory[5] = 0xC9; 

    /* running stuff! */
    for (;;)
    {
        /* override CALL instruction */
        if (i8080_state->memory[i8080_state->pc] == 0xCD)
        {
            addr = (uint16_t) i8080_state->memory[i8080_state->pc + 1] + \
                   (uint16_t) (i8080_state->memory[i8080_state->pc + 2] << 8);

            if (addr == 5)
            {
                if (i8080_state->c == 9)
                {
                    uint16_t offset = (i8080_state->d<<8) | (i8080_state->e);
                    unsigned char *str = &i8080_state->memory[offset];

                    while (*str != '$')
                         printf("%c", *str++); 
                }
                if (i8080_state->c == 2) putchar((char) i8080_state->e);

                i8080_state->pc += 3;

                continue;
            }      
            else if (addr == 0)
                    break;
        }

        if (i8080_run())
            break;

        if (i8080_state->pc == 0x0000)
            break;

    }

    return; 
}
