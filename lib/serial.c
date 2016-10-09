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

#include "interrupt.h"
#include "mmu.h"
#include "serial.h"
#include "cycles.h"

/* pointer to interrupt flags (handy) */
// interrupts_flags_t *serial_if;

/* pointer to serial controller register */
// serial_ctrl_t *serial_ctrl;

/* pointer to FF01 data */
// uint8_t *serial_data;

serial_t serial;

interrupts_flags_t *serial_if;

/* sent bits */
// uint8_t serial_bits_sent = 0;

// int serial_cnt = 0;

void serial_init_pointers()
{
    /* pointer to data to send/received */
//    serial.data = mmu_addr(0xFF01);

    /* assign timer values to them memory addresses */
//    serial.ctrl = mmu_addr(0xFF02);

    /* pointer to interrupt flags */
    // serial.ifp   = mmu_addr(0xFF0F);
}

void serial_init()
{
    /* pointer to interrupt flags */
    //serial_init_pointers();    

    serial_if = mmu_addr(0xFF0F);

    /* init counters */
    //serial.cnt = 0;
    serial.bits_sent = 0;
}

void serial_step()
{
    if (serial.clock && serial.transfer_start)
    {
//        serial.cnt += 4;

        /* TODO - check SGB mode, it could run at different clock */
        if (serial.next == cycles.cnt)
        {
	    serial.next += 256;

            /* reset counter */
//            serial.cnt = 0;

            /* if (serial.bits_sent == 0)
                printf("VOGLIO SPEDIRE %02x\n", *(serial.data)); */

            /* one bit more was sent - update FF01  */
            serial.data = (serial.data << 1) | 0x01;

            /* increase bit sent */
            serial.bits_sent++;

            /* reached 8 bits? */
            if (serial.bits_sent == 8)
            {
                /* reset counter */
                serial.bits_sent = 0;

                /* reset transfer_start flag to yell I'M DONE */
                serial.transfer_start = 0;

                /* and finally, trig the fucking interrupt */
                serial_if->serial_io = 1;
            }
        }
    }
}

void serial_save_stat(FILE *fp)
{
    fwrite(&serial, 1, sizeof(serial_t), fp);
}

void serial_restore_stat(FILE *fp)
{
    fread(&serial, 1, sizeof(serial_t), fp);

    serial_init_pointers();
}

void serial_write_reg(uint16_t a, uint8_t v)
{
    switch (a)
    {
        case 0xFF01: serial.data = v; return;
        case 0xFF02: serial.clock = v & 0x01; 
                     serial.speed = (v & 0x02) ? 0x01 : 0x00;
                     serial.spare = ((v >> 2) & 0x1F);
                     serial.transfer_start = (v & 0x80) ? 0x01 : 0x00;
    }


    if (serial.transfer_start)
        serial.next = cycles.cnt + 256;
}

uint8_t serial_read_reg(uint16_t a)
{
    switch (a)
    {
        case 0xFF01: return serial.data;
        case 0xFF02: return ((serial.clock) ? 0x01 : 0x00) |
                            ((serial.speed) ? 0x02 : 0x00) |
                            (serial.spare << 2)            |
                            ((serial.transfer_start) ? 0x80 : 0x00); 
    }

    return 0xFF;
}
