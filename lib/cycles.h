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

#ifndef __CYCLES_HDR__
#define __CYCLES_HDR__

#include <stdint.h>
#include <stdio.h>

typedef struct cycles_s
{
    /* am i init'ed? */
    uint_fast32_t          inited;
    
    /* ticks counter */
    uint_fast32_t          cnt;

    /* CPU clock */
    uint_fast32_t          clock;

    /* handy for calculation */
    uint_fast32_t          next;

    /* step varying on cpu and emulation speed */
    uint_fast32_t          step;

    /* total running seconds */
    uint_fast32_t          seconds;

    /* 2 spares */
    uint_fast32_t          spare;
    uint_fast32_t          spare2;

} cycles_t;

extern cycles_t cycles;

/* prototypes */
void cycles_change_emulation_speed();
char cycles_init();
void cycles_restore_stat(FILE *fp);
void cycles_save_stat(FILE *fp);
void cycles_set_speed(char dbl);
char cycles_start_timer();
void cycles_step();
void cycles_stop_timer();
void cycles_term();

#endif
