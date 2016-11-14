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

#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include "cycles.h"
#include "global.h"
#include "gpu.h"
#include "mmu.h"
#include "serial.h"
#include "sound.h"
#include "timer.h"
#include "interrupt.h"
#include "utils.h"

/* timer stuff */
struct itimerspec cycles_timer;
timer_t           cycles_timer_id = 0;
struct sigevent   cycles_te;
struct sigaction  cycles_sa;

interrupts_flags_t *cycles_if;

/* instance of the main struct */
cycles_t cycles = { 0, 0, 0, 0 };

#define CYCLES_PAUSES 1024

/* sync timing */
struct timespec deadline;

/* hard sync stuff (for remote connection) */
uint8_t  cycles_hs_mode = 0;


/* set hard sync mode. sync is given by the remote peer + local timer */
void cycles_start_hs()
{
    utils_log("Hard sync mode ON\n");

    /* boolean set to on */
    cycles_hs_mode = 1;
}

void cycles_stop_hs()
{
    utils_log("Hard sync mode OFF\n");

    /* boolean set to on */
    cycles_hs_mode = 0;
}

/* set double or normal speed */
void cycles_set_speed(char dbl)
{
    /* set global */
    global_cpu_double_speed = dbl;

    /* update clock */
    if (global_cpu_double_speed)
        cycles.clock = 4194304 * 2;
    else
        cycles.clock = 4194304;

    /* calculate the mask */
    cycles_change_emulation_speed();
} 

/* set emulation speed */
void cycles_change_emulation_speed()
{
    switch (global_emulation_speed)
    {
        case GLOBAL_EMULATION_SPEED_QUARTER:
            cycles.step = ((4194304 / CYCLES_PAUSES) / 4 
                          << global_cpu_double_speed);
            break;
        case GLOBAL_EMULATION_SPEED_HALF:
            cycles.step = ((4194304 / CYCLES_PAUSES) / 2 
                          << global_cpu_double_speed);
            break;
        case GLOBAL_EMULATION_SPEED_NORMAL:
            cycles.step = ((4194304 / CYCLES_PAUSES) 
                          << global_cpu_double_speed);
            break;
        case GLOBAL_EMULATION_SPEED_DOUBLE:
            cycles.step = ((4194304 / CYCLES_PAUSES) * 2 
                          << global_cpu_double_speed);
            break;
        case GLOBAL_EMULATION_SPEED_4X:
            cycles.step = ((4194304 / CYCLES_PAUSES) * 4
                          << global_cpu_double_speed);
            break;
    }
}

/* this function is gonna be called every M-cycle = 4 ticks of CPU */
void cycles_step()
{
    cycles.cnt += 4;

    /* 65536 == cpu clock / CYCLES_PAUSES pauses every second */
    if (cycles.cnt == cycles.next) 
    {
        deadline.tv_nsec += 1000000000 / CYCLES_PAUSES;

        if (deadline.tv_nsec > 1000000000)
        {
            deadline.tv_sec += 1;
            deadline.tv_nsec -= 1000000000;
        }

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
        
        cycles.next += cycles.step;

        /* update current running seconds */
        if (cycles.cnt % cycles.clock == 0)
            cycles.seconds++;
    }

    /* hard sync next step */
    if (cycles.cnt == cycles.hs_next)
    {
        /* set cycles for hard sync */
//        cycles.hs_next += (17556 << global_cpu_double_speed);
        cycles.hs_next += (4096 << global_cpu_double_speed);

        /* hard sync is on? */
        if (cycles_hs_mode)
        {
            /* send my status and wait for peer status back */
            serial_send_byte(serial.data, serial.clock, serial.transfer_start);

            /* wait for reply */
            serial_wait_data();

            /* verify if we need to trigger an interrupt */
            serial_verify_intr();
        }
    }

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
    if (serial.next == cycles.cnt)
    {
        /* nullize serial next */
        serial.next -= 1;

        /* reset counter */
        serial.bits_sent = 0;

        /* gotta reply with 0xff when asking for ff01 */
        serial.data = 0xFF;

        /* reset transfer_start flag to yell I'M DONE */
        serial.transfer_start = 0;

        /* if not connected, trig the fucking interrupt */
        cycles_if->serial_io = 1;
    }    
}

/* things to do when vsync kicks in */
void cycles_vsync()
{
    return;

    /* hard sync is on? */
    if (cycles_hs_mode)
    {
        /* send my status and wait for peer status back */
        serial_send_byte(serial.data, serial.clock, serial.transfer_start);

        /* wait for reply */
        serial_wait_data();

        /* verify if we need to trigger an interrupt */
        serial_verify_intr();
    }
}

char cycles_init()
{
    /* CLOCK */
    clock_gettime(CLOCK_MONOTONIC, &deadline);

    cycles.inited = 1;

    /* interrupt registers */
    cycles_if = mmu_addr(0xFF0F);

    /* init clock and counter */
    cycles.clock = 4194304;
    cycles.cnt = 0;
    cycles.hs_next = 70224;

    /* mask for pauses cycles fast calc */
    cycles.step = 4194304 / CYCLES_PAUSES;
    cycles.next = 4194304 / CYCLES_PAUSES;

    return 0;
}

char cycles_start_timer()
{
    /* just pick new time reference */
    clock_gettime(CLOCK_MONOTONIC, &deadline);

    return 0;
}

void cycles_stop_timer()
{
    if (cycles.inited == 0)
        return;
}

void cycles_term()
{
}

void cycles_save_stat(FILE *fp)
{
    fwrite(&cycles, 1, sizeof(cycles_t), fp);
}

void cycles_restore_stat(FILE *fp)
{
    fread(&cycles, 1, sizeof(cycles_t), fp);

    /* recalc speed stuff */
    cycles_change_emulation_speed();
}

