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

#include <errno.h>
#include <semaphore.h>
#include <signal.h>
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

/* am i init'ed? */
char              cycles_inited = 0;

/* ticks counter */
uint32_t          cycles_cnt;

/* CPU clock */
uint32_t          cycles_clock;

/* handy for .... pfff */
uint32_t          cycles_mask;

#define CYCLES_PAUSES 128

/* internal prototype for timer handler */
void cycles_timer_handler(int sig, siginfo_t *si, void *uc);


/* set double or normal speed */
void cycles_set_speed(char dbl)
{
    if (dbl)
        cycles_mask = 0xFFFF;
    else
        cycles_mask = 0x7FFF;
} 

/* set emulation speed */
void cycles_change_emulation_speed()
{
    switch (global_emulation_speed)
    {
        case GLOBAL_EMULATION_SPEED_HALF:
            cycles_mask = ((0x3FFF << global_cpu_double_speed) | 
                           global_cpu_double_speed);
            break;
        case GLOBAL_EMULATION_SPEED_NORMAL:
            cycles_mask = ((0x7FFF << global_cpu_double_speed) | 
                           global_cpu_double_speed);
            break;
        case GLOBAL_EMULATION_SPEED_DOUBLE:
            cycles_mask = ((0xFFFF << global_cpu_double_speed) | 
                           global_cpu_double_speed);
            break;
    }
}

/* this function is gonna be called every M-cycle = 4 ticks of CPU */
void cycles_step()
{
    cycles_cnt += 4;

    /* 65536 == cpu clock / CYCLES_PAUSES pauses every second */
    if ((cycles_cnt & cycles_mask) == 0)
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
    mmu_step();

    /* update GPU state */
    gpu_step();

    /* update timer state */
    timer_step();

    /* update serial state */
    serial_step();

    /* and finally sound */
    sound_step();
}

char cycles_init()
{
    cycles_inited = 1;

    /* init clock and counter */
    cycles_clock = 4194304;
    cycles_cnt = 0;

    /* mask for pauses cycles fast calc */
    cycles_mask = 0x7FFF;

    /* init semaphore for cpu clocks sync */
    sem_init(&cycles_sem, 0, 0);

    /* start timer */
    return cycles_start_timer();
}

char cycles_start_timer()
{
    if (cycles_inited == 0) 
        return 0;

    /* prepare timer to emulate video refresh interrupts */
    cycles_sa.sa_flags = SA_SIGINFO;
    cycles_sa.sa_sigaction = cycles_timer_handler;
    sigemptyset(&cycles_sa.sa_mask);
    // bzero(&cycles_sa.sa_mask, sizeof(sig_t));
    if (sigaction(SIGRTMIN, &cycles_sa, NULL) == -1)
        return 1;
    bzero(&cycles_te, sizeof(struct sigevent));

    /* set and enable alarm */
    cycles_te.sigev_notify = SIGEV_SIGNAL;
    cycles_te.sigev_signo = SIGRTMIN;
    cycles_te.sigev_value.sival_ptr = &cycles_timer_id;
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
    if (cycles_inited == 0)
        return;

    timer_delete(cycles_timer_id);
}

/* callback for timer events (64 times per second) */
void cycles_timer_handler(int sig, siginfo_t *si, void *uc)
{
    int sval;

    sem_getvalue(&cycles_sem, &sval);

    if (sval)
        printf("SVAL: %d\n", sval);

    sem_post(&cycles_sem);
}

void cycles_term()
{
    if (cycles_inited == 0)
        return;

    /* stop timer */
    timer_delete(cycles_timer_id);

    /* post it in case it was stuck */
    sem_post(&cycles_sem);

    /* destroy semaphore */
    sem_destroy(&cycles_sem);
}
