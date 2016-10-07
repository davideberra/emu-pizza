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
    cycles.inited = 1;

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
//void cycles_timer_handler(int sig, siginfo_t *si, void *uc)
void cycles_timer_handler(int sigval)
{
//    int sval;

/*    sem_getvalue(&cycles_sem, &sval);

    if (sval)
        printf("SVAL: %d\n", sval); */

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

//    printf("MEDIA %lld MILLISEX\n", totaru / totaru_cnt); 
}

void cycles_save_stat(FILE *fp)
{
    fwrite(&cycles, 1, sizeof(cycles_t), fp);
}

void cycles_restore_stat(FILE *fp)
{
    fread(&cycles, 1, sizeof(cycles_t), fp);
}

