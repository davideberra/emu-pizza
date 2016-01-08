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


#ifndef I8080_H
#define I8080_H

#include <stdint.h>

/* structs emulating 8080 registers and flags */
typedef struct i8080_flags_s
{
    uint8_t  z:1;
    uint8_t  s:1;
    uint8_t  p:1;
    uint8_t  cy:1;
    uint8_t  ac:1;
    uint8_t  pad:3;
} i8080_flags_t;

typedef struct i8080_state_s
{
    uint8_t        a;
    uint8_t        b;
    uint8_t        c;
    uint8_t        d;
    uint8_t        e;
    uint8_t        h;
    uint8_t        l;
    uint16_t       sp;
    uint16_t       pc;
    i8080_flags_t  flags;
    uint8_t        int_enable;
    uint8_t        memory[65536];
} i8080_state_t;

/* defines    */
#define I8080_MAX_MEMORY 65536

/* prototypes */
i8080_state_t *i8080_init(void);
int            i8080_run(void);
int            i8080_execute(unsigned char code);

#endif
