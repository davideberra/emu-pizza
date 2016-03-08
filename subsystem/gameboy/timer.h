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

#ifndef __TIMER__
#define __TIMER__

#include "subsystem/gameboy/interrupts_hdr.h"
#include "subsystem/gameboy/mmu_hdr.h"
#include "subsystem/gameboy/timer_hdr.h"

/* pointer to interrupt flags (handy) */
interrupts_flags_t *timer_if;

void timer_init()
{
    /* assign timer values to them memory addresses */
    timer.div  = mmu_addr(0xFF04); 
    timer.cnt  = mmu_addr(0xFF05); 
    timer.mod  = mmu_addr(0xFF06); 
    timer.ctrl = mmu_addr(0xFF07); 

    timer.sub = 0;
	
    /* pointer to interrupt flags */
    timer_if   = mmu_addr(0xFF0F);
}

/* update timer internal state given CPU T-states */
void static __always_inline timer_step(uint8_t t)
{
    /* div_sub always run */
    timer.div_sub += t;

    if (timer.div_sub >= 256)
    {
        timer.div_sub -= 256;
        (*timer.div)++;
    }

    /* timer is on? */
    if ((*(timer.ctrl) & 0x04) == 0)
        return;

    /* add t to current sub */
    timer.sub += t;

    /* save value */
    uint16_t cnt = *(timer.cnt);

    /* calc threshold */
    uint16_t threshold;

    switch (*(timer.ctrl) & 0x03)
    {
        case 0x00: threshold = 1024; break;
        case 0x01: threshold = 16; break; 
        case 0x02: threshold = 64; break; 
        case 0x03: threshold = 256; break; 
    }

    /* threshold span overtaken? increment cnt value */
    if (timer.sub >= threshold)
    {
        timer.sub -= threshold;
        cnt++;
    }

    /* cnt value > 255? trigger an interrupt */
    if (cnt > 255)
    {
        cnt = *(timer.mod);

        /* trigger timer interrupt */
        timer_if->timer = 1;
    }

    *(timer.cnt) = cnt;
}


#endif 
