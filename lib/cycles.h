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

/* global */
extern uint32_t          cycles_clock;

/* prototypes */
char cycles_init();
void cycles_set_speed(char dbl);
char cycles_start_timer();
void cycles_step();
void cycles_stop_timer();
void cycles_term();

#endif
