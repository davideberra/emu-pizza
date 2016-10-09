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

#include "cycles.h"
#include "interrupt.h"
#include "mmu.h"
#include "timer.h"

/* pointer to interrupt flags (handy) */
interrupts_flags_t *timer_if;


void timer_init()
{
    /* assign timer values to them memory addresses */
/*    timer.div  = mmu_addr(0xFF04); 
    timer.cnt  = mmu_addr(0xFF05); 
    timer.mod  = mmu_addr(0xFF06); 
    timer.ctrl = mmu_addr(0xFF07); */

    timer.next = cycles.cnt + 256;
    timer.sub = 0;
	
    /* pointer to interrupt flags */
    timer_if   = mmu_addr(0xFF0F);
}

/* update timer internal state given CPU T-states */
void timer_step()
{
    /* div_sub always run */
//    timer.div_sub += 4;
//    if ((timer.div_sub & 0x000000FF) == 0x00)

    if (cycles.cnt == timer.next)
    {
        timer.next += 256;
        timer.div++;
    }

    /* timer is on? */
    if ((timer.ctrl & 0x04) == 0)
        return;

    /* add t to current sub */
    timer.sub += 4;

    /* save value */
    uint16_t cnt = timer.cnt;

    /* calc threshold */
/*    uint16_t threshold;

    switch (timer.ctrl & 0x03)
    {
        case 0x00: threshold = 1024; break;
        case 0x01: threshold = 16; break; 
        case 0x02: threshold = 64; break; 
        case 0x03: threshold = 256; break; 
    } */

    /* threshold span overtaken? increment cnt value */
    if (timer.sub >= timer.threshold)
    {
        timer.sub -= timer.threshold;
        cnt++;
    }

    /* cnt value > 255? trigger an interrupt */
    if (cnt > 255)
    {
        cnt = timer.mod;

        /* trigger timer interrupt */
        timer_if->timer = 1;
    }

    timer.cnt = cnt;
}

void timer_write_reg(uint16_t a, uint8_t v)
{
    switch (a)
    {
        case 0xFF04: timer.div = 0; return;
        case 0xFF05: timer.cnt = v; return;
        case 0xFF06: timer.mod = v; return;
        case 0xFF07: timer.ctrl = v; 
   }

    switch (timer.ctrl & 0x03)
    {
        case 0x00: timer.threshold = 1024; break;
        case 0x01: timer.threshold = 16; break;
        case 0x02: timer.threshold = 64; break;
        case 0x03: timer.threshold = 256; break;
    }
}

uint8_t timer_read_reg(uint16_t a)
{
    switch (a)
    {
        case 0xFF04: return timer.div;
        case 0xFF05: return timer.cnt;
        case 0xFF06: return timer.mod;
        case 0xFF07: return timer.ctrl;
    }

    return 0xFF;
}


