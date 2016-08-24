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

#ifndef __SERIAL_HDR__
#define __SERIAL_HDR__

#include <stdint.h>

void serial_init();
void serial_step();

typedef struct serial_ctrl_s
{ 
    uint8_t clock:1;
    uint8_t speed:1;
    uint8_t spare:5;
    uint8_t transfer_start:1;
} serial_ctrl_t;

#endif
