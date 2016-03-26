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

#ifndef __SERIAL__
#define __SERIAL__

#include "subsystem/gameboy/interrupts_hdr.h"
#include "subsystem/gameboy/mmu_hdr.h"
#include "subsystem/gameboy/serial_hdr.h"

/* pointer to interrupt flags (handy) */
interrupts_flags_t *serial_if;

/* pointer to serial controller register */
serial_ctrl_t *serial_ctrl;

/* pointer to FF01 data */
uint8_t *serial_data;

/* sent bits */
uint8_t serial_bits_sent = 0;

int totaru = 0;

void serial_init()
{
    /* pointer to data to send/received */
    serial_data = mmu_addr(0xFF01);

    /* assign timer values to them memory addresses */
    serial_ctrl = mmu_addr(0xFF02);
        
    /* pointer to interrupt flags */
    serial_if   = mmu_addr(0xFF0F);
}

void static __always_inline serial_step(uint8_t t)
{
    if (serial_ctrl->transfer_start && serial_ctrl->clock)
    {
        totaru += t;

        /* TODO - check SGB mode, it could run at different clock */
        if (totaru >= 256)
        {
            /* reset counter */
            totaru = 0;

            /* one bit more was sent - update FF01  */
            *serial_data = (*serial_data << 1) | 0x01;

            /* increase bit sent */
            serial_bits_sent++;

            /* reached 8 bits? */
            if (serial_bits_sent == 8)
            {
                /* reset counter */
                serial_bits_sent = 0;

                /* reset transfer_start flag to yell I'M DONE */
                serial_ctrl->transfer_start = 0;

                /* and finally, trig the fucking interrupt */
                serial_if->serial_io = 1;
            }
        }
    }
}


#endif 
