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

#ifndef __CYCLES__
#define __CYCLES__

#include "subsystem/gameboy/gpu_hdr.h"
#include "subsystem/gameboy/timer_hdr.h"

void static __always_inline cycles_step(uint8_t s)
{
    /* update GPU state */
    gpu_step(s);

    /* update timer state */
    timer_step(s);
}

#endif
