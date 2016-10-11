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

/* main variable */
serial_t serial;

interrupts_flags_t *serial_if;

void serial_init()
{
    /* pointer to interrupt flags */
    serial_if = mmu_addr(0xFF0F);

    /* init counters */
    serial.bits_sent = 0;
}

void serial_save_stat(FILE *fp)
{
    fwrite(&serial, 1, sizeof(serial_t), fp);
}

void serial_restore_stat(FILE *fp)
{
    fread(&serial, 1, sizeof(serial_t), fp);
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
