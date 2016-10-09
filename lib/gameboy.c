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
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "cartridge.h"
#include "sound.h"
#include "mmu.h"
#include "cycles.h"
#include "gpu.h"
#include "global.h"
#include "input.h"
#include "timer.h"
#include "serial.h"
#include "z80_gameboy_regs.h"
#include "z80_gameboy.h"

/* semaphore for pauses */
sem_t gameboy_sem;

char gameboy_inited = 0;


void gameboy_init()
{
    /* init global values */
    // global_init(); 

    /* init z80 */
    z80_init(); 

    /* init cycles syncronizer */
    cycles_init();

    /* init input */
    input_init();

    /* init timer */
    timer_init();

    /* init serial */
    serial_init();

    /* init sound (this will start audio thread) */
    sound_init();

    /* reset to default values */
    mmu_write_no_cyc(0xFF05, 0x00);
    mmu_write_no_cyc(0xFF06, 0x00); 
    mmu_write_no_cyc(0xFF07, 0x00);
    mmu_write_no_cyc(0xFF10, 0x80); 
    mmu_write_no_cyc(0xFF11, 0xBF); 
    mmu_write_no_cyc(0xFF12, 0xF3); 
    mmu_write_no_cyc(0xFF14, 0xBF);
    mmu_write_no_cyc(0xFF16, 0x3F);
    mmu_write_no_cyc(0xFF17, 0x00); 
    mmu_write_no_cyc(0xFF19, 0xBF); 
    mmu_write_no_cyc(0xFF1A, 0x7F); 
    mmu_write_no_cyc(0xFF1B, 0xFF);
    mmu_write_no_cyc(0xFF1C, 0x9F); 
    mmu_write_no_cyc(0xFF1E, 0xBF);
    mmu_write_no_cyc(0xFF20, 0xFF); 
    mmu_write_no_cyc(0xFF21, 0x00);
    mmu_write_no_cyc(0xFF22, 0x00); 
    mmu_write_no_cyc(0xFF23, 0xBF); 
    mmu_write_no_cyc(0xFF24, 0x77);
    mmu_write_no_cyc(0xFF25, 0xF3); 
    mmu_write_no_cyc(0xFF26, 0xF1);
    mmu_write_no_cyc(0xFF40, 0x91);
    mmu_write_no_cyc(0xFF41, 0xF0);
    mmu_write_no_cyc(0xFF42, 0x00);
    mmu_write_no_cyc(0xFF43, 0x00);  
    mmu_write_no_cyc(0xFF44, 0x00);  
    mmu_write_no_cyc(0xFF45, 0x00); 
    mmu_write_no_cyc(0xFF47, 0xFC); 
    mmu_write_no_cyc(0xFF48, 0xFF); 
    mmu_write_no_cyc(0xFF49, 0xFF); 
    mmu_write_no_cyc(0xFF4A, 0x00); 
    mmu_write_no_cyc(0xFF4B, 0x00); 
    mmu_write_no_cyc(0xFFFF, 0x00);  
    mmu_write_no_cyc(0xC000, 0x08);
    mmu_write_no_cyc(0xFFFE, 0x69);

    if (global_cgb)
        state.a = 0x11;
    else
        state.a = 0x00;

    state.b = 0x00;
    state.c = 0x13;
    state.d = 0x00;
    state.e = 0xd8;
    state.h = 0x01;
    state.l = 0x4d;
    state.pc = 0x0100;
    state.sp = 0xFFFE;
    *state.f = 0xB0;

    /* init semaphore for pauses */
    sem_init(&gameboy_sem, 0, 0);

    /* mark as inited */
    gameboy_inited = 1;

    return;
} 

void gameboy_set_pause(char pause)
{
    if (!gameboy_inited)
        return;

    if (pause == global_pause)
        return;

    global_pause = pause;

    if (pause)
    {
        /* stop timer */
        cycles_stop_timer();
    }
    else
    {
        /* restart timer */
        cycles_start_timer();

        /* wake up */
        sem_post(&gameboy_sem);
    }
}

void gameboy_run()
{
    uint8_t op;

    /* init */
    // gameboy_init();

    /* reset counter */
    cycles.cnt = 0;

    /* get interrupt flags and interrupt enables */
    uint8_t *int_e;
    uint8_t *int_f;

    /* pointers to memory location of interrupt enables/flags */
    int_e = mmu_addr(0xFFFF);
    int_f = mmu_addr(0xFF0F);

    /* start at normal speed */
    global_cpu_double_speed = 0;

    /* run stuff!                                                          */
    /* mechanism is simple.                                                */
    /* 1) execute instruction 2) update cycles counter 3) check interrupts */
    /* and repeat forever                                                  */
    while (!global_quit)
    {
        if (global_slow_down)
        {
            usleep(100000);
            global_slow_down = 0;
        }

        /* pause? */
        while (global_pause)
            sem_wait(&gameboy_sem);

        /* get op */
        op   = mmu_read(state.pc);

        /* print out CPU state if enabled by debug flag */
        if (global_debug)
        {
            printf("OP: %02x F: %02x PC: %04x:%02x:%02x SP: %04x:%02x:%02x ",
                                   op, *state.f & 0xd0, state.pc, 
                                   mmu_read_no_cyc(state.pc + 1),
                                   mmu_read_no_cyc(state.pc + 2), state.sp,
                                   mmu_read_no_cyc(state.sp), 
                                   mmu_read_no_cyc(state.sp + 1));


            printf("A: %02x BC: %04x DE: %04x HL: %04x\n", state.a, *state.bc,
                                                          *state.de, *state.hl);
        }

        /* execute instruction by the GB Z80 version */
        z80_execute(op);

        /* keep low bits always set to zero */
        // *state.f &= 0xf0;

        /* if last op was Interrupt Enable (0xFB)  */
        /* we need to check for INTR on next cycle */
        if (op == 0xFB)
            continue;

        /* interrupts filtered by enable flags */
        uint8_t int_r = (*int_f & *int_e);

        /* check for interrupts */
        if ((state.int_enable || op == 0x76) && (int_r != 0))
        {
            /* beware of instruction that doesn't move PC! */
            /* like HALT (0x76)                            */
            if (op == 0x76)
            {
                state.pc++;

                if (state.int_enable == 0)
                    continue;
            }

            /* reset int-enable flag, it will be restored after a RETI op */
            state.int_enable = 0;

            if ((int_r & 0x01) == 0x01)
            {
                /* vblank interrupt triggers RST 5 */

                /* reset flag */
                *int_f &= 0xFE;

                /* handle the interrupt */
                z80_intr(0x0040); 
            }
            else if ((int_r & 0x02) == 0x02)
            {
                /* LCD Stat interrupt */

                /* reset flag */
                *int_f &= 0xFD;

                /* handle the interrupt! */
                z80_intr(0x0048); 
            }       
            else if ((int_r & 0x04) == 0x04)
            {
                /* timer interrupt */

                /* reset flag */
                *int_f &= 0xFB;

                /* handle the interrupt! */
                z80_intr(0x0050); 
            } 
            else if ((int_r & 0x08) == 0x08)
            {
                /* serial interrupt */

                /* reset flag */
                *int_f &= 0xF7;

                /* handle the interrupt! */
                z80_intr(0x0058); 
            } 
        }
    }

    /* when ended... save RAM */
    
    //mmu_save_ram(file_sav);

    /* terminate all the stuff */
    cartridge_term();
    sound_term();
    mmu_term();

    return; 
}

void gameboy_stop() 
{
    global_quit = 1;

    /* wake up */
    if (global_pause) 
    {
        global_pause = 0;
        sem_post(&gameboy_sem);
    }

    /* unlock threads stuck during reading */
    sound_term();

    /* shutdown semaphore limitator */
    cycles_term();
}

char gameboy_restore_stat(int idx)
{
    char path[256];

    /* ensure i'm in pause */
    gameboy_set_pause(1);

    /* wait a little, just to ensure main loop enter in pause state */
    usleep(100000);

    /* build output file name */
    snprintf(path, sizeof(path), "%s/%s.%d.stat", global_save_folder,
                                                  global_rom_name, idx);

    FILE *fp = fopen(path, "r+");

    if (fp == NULL)
        return 1;

    /* restore CPU status */
    fread(&state, 1, sizeof(z80_state_t), fp);

    state.f = (uint8_t *) &state.flags;
    state.bc = (uint16_t *) &state.c;
    state.de = (uint16_t *) &state.e;
    state.hl = (uint16_t *) &state.l;

    /* dump every module */
    sound_restore_stat(fp);
    gpu_restore_stat(fp);
    serial_restore_stat(fp);
    mmu_restore_stat(fp);
    cycles_restore_stat(fp);

    fclose(fp);

    return 0;
}

char gameboy_save_stat(int idx)
{
    char path[256];

    /* ensure i'm in pause */
    gameboy_set_pause(1);

    /* wait a little, just to ensure main loop enter in pause state */
    usleep(100000);

    /* build output file name */
    snprintf(path, sizeof(path), "%s/%s.%d.stat", global_save_folder, 
                                                  global_rom_name, idx);

    FILE *fp = fopen(path, "w+");

    if (fp == NULL)
        return 1;

    /* dump cpu status */
    fwrite(&state, 1, sizeof(z80_state_t), fp);

    /* dump every module */
    sound_save_stat(fp);
    gpu_save_stat(fp);
    serial_save_stat(fp);
    mmu_save_stat(fp);
    cycles_save_stat(fp);

    fclose(fp);

    /* now dump raw data of frame buffer */
    snprintf(path, sizeof(path), "%s/%s.%d.fb", global_save_folder, 
                                                global_rom_name, idx);

    fp = fopen(path, "w+");

    if (fp == NULL)
        return 1;

    /* dump frame buffer pixels */
    gpu_save_fb(fp);

    fclose(fp);
    
    return 0;
}

