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
#include "global.h"
#include "gpu.h"
#include "mmu.h"
#include "serial.h"
#include "sound.h"
#include "timer.h"
#include "interrupt.h"

#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

/* timer stuff */
struct itimerspec cycles_timer;
timer_t           cycles_timer_id = 0;
sem_t             cycles_sem;
struct sigevent   cycles_te;
struct sigaction  cycles_sa;

interrupts_flags_t *cycles_if;

/* instance of the main struct */
cycles_t cycles = { 0, 0, 0, 0 };

#define CYCLES_PAUSES 64

/* internal prototype for timer handler */
void cycles_timer_handler(int sigval);


/* set double or normal speed */
void cycles_set_speed(char dbl)
{
    /* set global */
    global_cpu_double_speed = dbl;

    /* calculate the mask */
    cycles_change_emulation_speed();
} 

/* set emulation speed */
void cycles_change_emulation_speed()
{
    switch (global_emulation_speed)
    {
        case GLOBAL_EMULATION_SPEED_HALF:
            cycles.mask = (uint32_t) ((0x00007FFF << global_cpu_double_speed) |
                                      global_cpu_double_speed);
            break;
        case GLOBAL_EMULATION_SPEED_NORMAL:
            cycles.mask = (uint32_t) ((0x0000FFFF << global_cpu_double_speed) |
                                      global_cpu_double_speed);
            break;
        case GLOBAL_EMULATION_SPEED_DOUBLE:
            cycles.mask = (uint32_t) ((0x0001FFFF << global_cpu_double_speed) |
                                      global_cpu_double_speed);
            break;
    }
}

/* this function is gonna be called every M-cycle = 4 ticks of CPU */
void cycles_step()
{
    cycles.cnt += 4;

    /* 65536 == cpu clock / CYCLES_PAUSES pauses every second */
    if ((cycles.cnt & cycles.mask) == 0x00000000)
    {
        int res = 0;

        while (1)
        {
            res = sem_wait(&cycles_sem);

            if (res == -1)
            {
                if (errno == EINTR)
                    continue;
            }

            break;
        }
    }

    /* update memory state (for DMA) */

    /* DMA */
    if (mmu.dma_address != 0x0000)
    {
        mmu.dma_cycles -= 4;

        /* enough cycles passed? */
        if (mmu.dma_cycles == 0)
        {
            memcpy(&mmu.memory[0xFE00], &mmu.memory[mmu.dma_address], 160);

            /* reset address */
            mmu.dma_address = 0x0000;
        }
    }

    /* HDMA (only CGB) */
    if (global_cgb && mmu.hdma_to_transfer)
    {
        /* hblank transfer */
        if (mmu.hdma_transfer_mode)
        {
            /* transfer when line is changed and we're into HBLANK phase */
            if (mmu.memory[0xFF44] < 143 &&
                mmu.hdma_current_line != mmu.memory[0xFF44] &&
               (mmu.memory[0xFF41] & 0x03) == 0x00)
            {
                /* update current line */
                mmu.hdma_current_line = mmu.memory[0xFF44];

                /* copy 0x10 bytes */
                if (mmu.vram_idx)
                    memcpy(mmu_addr_vram1() + mmu.hdma_dst_address - 0x8000,
                           &mmu.memory[mmu.hdma_src_address], 0x10);
                else
                    memcpy(mmu_addr_vram0() + mmu.hdma_dst_address - 0x8000,
                           &mmu.memory[mmu.hdma_src_address], 0x10);

                /* decrease bytes to transfer */
                mmu.hdma_to_transfer -= 0x10;

                /* increase pointers */
                mmu.hdma_dst_address += 0x10;
                mmu.hdma_src_address += 0x10;
            }
        }
    }

    /* update GPU state */

    /* advance only in case of turned on display */
    if ((*gpu.lcd_ctrl).display)
        if (gpu.next == cycles.cnt)
            gpu_step();

    /* and proceed with sound step */
    if (sound.channel_three.ram_access)
        sound.channel_three.ram_access -= 4;
        
    /* fs clock */
    if (sound.fs_cycles_next == cycles.cnt)
        sound_step_fs();
        
    /* channel one */
    if (sound.channel_one.active && 
        sound.channel_one.duty_cycles_next == cycles.cnt)
        sound_step_ch1();

    /* channel two */
    if (sound.channel_two.active && 
        sound.channel_two.duty_cycles_next == cycles.cnt)
        sound_step_ch2();
        
    /* channel three */
    if (sound.channel_three.active && 
        sound.channel_three.cycles_next <= cycles.cnt)
        sound_step_ch3();        
        
    /* channel four */
    if (sound.channel_four.active && 
        sound.channel_four.cycles_next == cycles.cnt)
        sound_step_ch4();
        
    /* time to generate a sample? */
    if (sound.sample_cycles_next_rounded == cycles.cnt)
        sound_step_sample();

    /* update timer state */
    if (cycles.cnt == timer.next)
    {
        timer.next += 256;
        timer.div++;
    }

    /* timer is on? */
    if (timer.active)
    {
        /* add t to current sub */
        timer.sub += 4;

        /* threshold span overtaken? increment cnt value */
        if (timer.sub == timer.threshold)
        {
            timer.sub -= timer.threshold;
            timer.cnt++;
            
            /* cnt value > 255? trigger an interrupt */
            if (timer.cnt > 255)
            {
                timer.cnt = timer.mod;

                /* trigger timer interrupt */
                cycles_if->timer = 1;
            }            
        }
    }

    /* update serial state */
    if (serial.clock && serial.transfer_start)
    {
        /* TODO - check SGB mode, it could run at different clock */
        if (serial.next == cycles.cnt)
        {
            serial.next += 256;

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
                cycles_if->serial_io = 1;
            }
        }
    }    
}

char cycles_init()
{
    cycles.inited = 1;

    /* interrupt registers */
    cycles_if = mmu_addr(0xFF0F);

    /* init clock and counter */
    cycles.clock = 4194304;
    cycles.cnt = 0;

    /* mask for pauses cycles fast calc */
    cycles.mask = 0xFFFF;

    /* init semaphore for cpu clocks sync */
    sem_init(&cycles_sem, 0, 0);

    /* prepare timer to emulate video refresh interrupts */
    cycles_sa.sa_flags = SA_SIGINFO;
    sigemptyset(&cycles_sa.sa_mask);

    if (sigaction(SIGRTMIN, &cycles_sa, NULL) == -1)
        return 1;
    bzero(&cycles_te, sizeof(struct sigevent));

    /* set and enable alarm */
    cycles_te.sigev_notify = SIGEV_THREAD;
    cycles_te.sigev_signo = SIGRTMIN;
    cycles_te.sigev_notify_function = (void *) cycles_timer_handler;
    cycles_te.sigev_value.sival_ptr = &cycles_timer_id;

    /* start timer */
    return cycles_start_timer();
}

char cycles_start_timer()
{
    if (cycles.inited == 0)
        return 0;

    timer_create(CLOCK_REALTIME, &cycles_te, &cycles_timer_id);

    /* initialize CYCLES_PAUSES hits per second timer */
    cycles_timer.it_value.tv_sec = 1;
    cycles_timer.it_value.tv_nsec = 1000;
    cycles_timer.it_interval.tv_sec = 0;
    cycles_timer.it_interval.tv_nsec = 1000000000 / CYCLES_PAUSES;

    /* start timer */
    timer_settime(cycles_timer_id, 0, &cycles_timer, NULL);

    return 0;
}

void cycles_stop_timer()
{
    if (cycles.inited == 0)
        return;

    timer_delete(cycles_timer_id);
}

/* callback for timer events (64 times per second) */
void cycles_timer_handler(int sigval)
{
    sem_post(&cycles_sem);
}

void cycles_term()
{
    if (cycles.inited == 0)
        return;

    /* stop timer */
    timer_delete(cycles_timer_id);

    /* post it in case it was stuck */
    sem_post(&cycles_sem);

    /* destroy semaphore */
    sem_destroy(&cycles_sem);
}

void cycles_save_stat(FILE *fp)
{
    fwrite(&cycles, 1, sizeof(cycles_t), fp);
}

void cycles_restore_stat(FILE *fp)
{
    fread(&cycles, 1, sizeof(cycles_t), fp);
}

