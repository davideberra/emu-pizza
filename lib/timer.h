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

#ifndef __TIMER_HDR__
#define __TIMER_HDR__

#include <stdint.h>

/* prototypes */
void timer_init();
void timer_step();

/* Gameboy Timer status */
typedef struct gameboy_timer_s
{
    /* divider - 0xFF04 */
    uint8_t *div;

    /* counter - 0xFF05 */
    uint8_t *cnt;

    /* modulo  - 0xFF06 */
    uint8_t *mod;

    /* control - 0xFF07 */
    uint8_t *ctrl;

    /* current value    */
    uint32_t sub;
    uint32_t div_sub;

} gameboy_timer_t;

/* global status of timer */
gameboy_timer_t timer;

#endif
