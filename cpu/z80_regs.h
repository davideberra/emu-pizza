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


#ifndef Z80_H
#define Z80_H

#include <stdint.h>

/* structs emulating 8080 registers and flags */
typedef struct z80_flags_s
{
    uint8_t  cy:1;
    uint8_t  n:1;
    uint8_t  p:1;
    uint8_t  u3:1;
    uint8_t  ac:1;
    uint8_t  u5:1;
    uint8_t  z:1;
    uint8_t  s:1;
} z80_flags_t;

typedef struct z80_state_s
{
    uint8_t        spare;
    uint8_t        a;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t        c;
    uint8_t        b;
#else
    uint8_t        b;
    uint8_t        c;
#endif
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t        e;
    uint8_t        d;
#else
    uint8_t        d;
    uint8_t        e;
#endif
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t        l;
    uint8_t        h;
#else
    uint8_t        h;
    uint8_t        l;
#endif
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t        ixl;
    uint8_t        ixh;
#else
    uint8_t        ixh;
    uint8_t        ixl;
#endif
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t        iyl;
    uint8_t        iyh;
#else
    uint8_t        iyh;
    uint8_t        iyl;
#endif
    uint16_t       sp;
    uint16_t       pc;

    /* shortcuts */
    uint16_t       *bc;
    uint16_t       *de;
    uint16_t       *hl;
    uint16_t       *ix;
    uint16_t       *iy;
    uint8_t        *f;

    uint8_t        memory[65536];
    uint32_t       cycles;

    uint32_t       skip_cycle;

    z80_flags_t    flags;
    uint8_t        int_enable;
    uint8_t        spare2;
    uint8_t        spare3;

} z80_state_t;

/* defines    */
#define Z80_MAX_MEMORY 65536

/* prototypes */
/*z80_state_t   *z80_init(void);
int            z80_run(void);
int            z80_execute(unsigned char code);
int            z80_disassemble(unsigned char *codebuffer, int pc);
void           z80_print_state(void);
*/

#endif
