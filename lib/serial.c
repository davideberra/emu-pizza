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

#include <pthread.h>

#include "cycles.h"
#include "interrupt.h"
#include "mmu.h"
#include "serial.h"
#include "utils.h"

/* main variable */
serial_t serial;

/* function to call when frame is ready */
serial_data_send_cb_t serial_data_send_cb;

interrupts_flags_t *serial_if;

/* semaphore for serial sync */
pthread_cond_t    serial_cond;
pthread_mutex_t   serial_mutex;


void serial_init()
{
    /* pointer to interrupt flags */
    serial_if = mmu_addr(0xFF0F);

    /* init counters */
    serial.bits_sent = 0;

    /* start as not connected */
    serial.peer_connected = 0;

    /* init semaphore for sync */
    pthread_mutex_init(&serial_mutex, NULL);
    pthread_cond_init(&serial_cond, NULL);
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
    /* lock the serial */
    pthread_mutex_lock(&serial_mutex);

    switch (a)
    {
    case 0xFF01: 
        serial.data = v; goto end;
    case 0xFF02: 
        serial.clock = v & 0x01; 
        serial.speed = (v & 0x02) ? 0x01 : 0x00;
        serial.spare = ((v >> 2) & 0x1F);
        serial.transfer_start = (v & 0x80) ? 0x01 : 0x00;

        /* reset? */
        serial.data_sent = 0;
    }

    if (serial.transfer_start)
    {
        serial.data_to_send = serial.data;

        if (serial.peer_connected)
        {
            if (serial.data_recv)
            {
                if (serial.data_recv_clock != serial.clock)
                    serial_send_byte(serial.data, serial.clock);
            }
            else
            {
                /* start only if clock is 1 */
                if (serial.clock)
                {
            /*        if (serial.speed)
                        serial.next = cycles.cnt + (8 * 8);
                    else
                        serial.next = cycles.cnt + (256 * 8); */
                    serial_send_byte(serial.data, serial.clock);
                }
            }
        }
        else if (serial.clock) 
        {
            if (serial.speed)
                serial.next = cycles.cnt + 8 * 8;
	    else
                serial.next = cycles.cnt + 256 * 8;
        }
    } 

end:

    /* lock the serial */
    pthread_mutex_unlock(&serial_mutex);
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

void serial_recv_byte(uint8_t v, uint8_t clock)
{
    /* lock the serial */
    pthread_mutex_lock(&serial_mutex);

    /* received side OK */
    serial.data_recv = 1;
    serial.data_recv_clock = clock;
    serial.data_to_recv = v;

    /* is it a respons ? */
    if (!serial.data_sent &&
         serial.transfer_start &&
         serial.clock != serial.data_recv_clock)
    {
        if (serial_data_send_cb)
            (*serial_data_send_cb) (serial.data_to_send, serial.clock);

        serial.data_sent = 1;
    }

    /* it's the first message of the send-recv couple? just save it */
    if (serial.data_sent == 0)
        goto end;

    /* received and sent! time to make data available */
    serial.data = v;

    /* reset */
    serial.data_sent = 0;
    serial.data_recv = 0;

    /* stop */
    serial.transfer_start = 0;
    serial.bits_sent = 0;

    /* trig an interrupt - this function is called *
     * after a byte has been sent and received     */
    serial_if->serial_io = 1;

end:

    /* lock the serial */
    pthread_mutex_unlock(&serial_mutex);
}

void serial_send_byte(uint8_t v, uint8_t clock)
{
    serial.data_sent = 1;

    if (serial_data_send_cb)
        (*serial_data_send_cb) (v, clock);

    if (clock)
        return;

    /* it's the first message of the send-recv couple? just save it */
    if (!serial.data_recv)
        return;

    /* extract last received message */
    serial.data = serial.data_to_recv;

    /* reset */
    serial.data_sent = 0;
    serial.data_recv = 0;

    /* stop */
    serial.transfer_start = 0;
    serial.bits_sent = 0;

    /* trig an interrupt - this function is called */
    /* after a byte has been sent and received     */
    serial_if->serial_io = 1;
}

void serial_set_send_cb(serial_data_send_cb_t cb)
{
    serial_data_send_cb = cb;
}
