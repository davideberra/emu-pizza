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
void serial_write_reg(uint16_t a, uint8_t v);
uint8_t serial_read_reg(uint16_t a);
void serial_save_stat(FILE *fp);
void serial_restore_stat(FILE *fp);

typedef struct serial_ctrl_s
{ 
    uint8_t clock;
    uint8_t speed;
    uint8_t spare;
    uint8_t transfer_start;
} serial_ctrl_t;

typedef struct serial_s {

    /* pointer to serial controller register */
    // serial_ctrl_t  ctrl;

    uint8_t clock;
    uint8_t speed;
    uint8_t spare;
    uint8_t transfer_start;

    /* pointer to FF01 data */
    uint8_t  data;

    /* sent bits */
    uint8_t  bits_sent;
    uint8_t  spare2;
    uint8_t  spare3;

    /* counter */
    uint_fast32_t next;

} serial_t;

extern serial_t serial;

#endif
