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

#ifndef __CYCLES__
#define __CYCLES__

#include "subsystem/gameboy/globals.h"
#include "subsystem/gameboy/gpu_hdr.h"
#include "subsystem/gameboy/serial_hdr.h"
#include "subsystem/gameboy/timer_hdr.h"

#include <sys/time.h>

/* timer */
struct itimerspec cycles_timer;
timer_t           cycles_timer_id = 0;
sem_t             cycles_sem;
struct sigevent   cycles_te;
struct sigaction  cycles_sa;

int prot =0;
int pret = 0;

#define CYCLES_PAUSES 128

/* internal prototype for timer handler */
void cycles_timer_handler(int sig, siginfo_t *si, void *uc);


/* this function is gonna be called every M-cycle = 4 ticks of CPU */
void static __always_inline cycles_step(uint8_t s)
{
    cycles_cnt += s;

    /* 65536 == cpu clock / CYCLES_PAUSES pauses every second */
    if (cycles_cnt % (cycles_clock / CYCLES_PAUSES) == 0)
    {
        int res = 0;

        pret++;

        while (global_benchmark == 0)
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

    if (cycles_cnt == cycles_clock)
    {
    /*    int sval;

        sem_getvalue(&cycles_sem, &sval);

        struct timeval te; 
        gettimeofday(&te, NULL);

        printf("%d.%d PRET %d PROT %d SVAL %d REQ %d PUSH %d\n", te.tv_sec, te.tv_usec / 1000, pret, prot, sval, 
         sound_req, sound_push); */

        sound_req =0;
        sound_push =0;

        pret = 0;
        prot = 0;

        cycles_cnt = 0;
    }

    /* update memory state (for DMA) */
    mmu_step(s);

    /* update GPU state */
    gpu_step(s);

    /* update timer state */
    timer_step(s);

    /* update serial state */
    serial_step(s);

    /* and finally sound */
    if (global_benchmark == 0)
        sound_step(s);
}

char cycles_init()
{
    /* init semaphore for cpu clocks sync */
    sem_init(&cycles_sem, 0, 0);

    /* prepare timer to emulate video refresh interrupts */
    cycles_sa.sa_flags = SA_SIGINFO;
    cycles_sa.sa_sigaction = cycles_timer_handler;
    sigemptyset(&cycles_sa.sa_mask);
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
    cycles_timer.it_value.tv_nsec = 0;
    cycles_timer.it_interval.tv_sec = 0;
    cycles_timer.it_interval.tv_nsec = 1000000000 / CYCLES_PAUSES;

    /* start timer */
    timer_settime(cycles_timer_id, 0, &cycles_timer, NULL);

    return 0;
}

/* callback for timer events (64 times per second) */
void cycles_timer_handler(int sig, siginfo_t *si, void *uc)
{
    prot++;

    int sval;

    sem_getvalue(&cycles_sem, &sval);

    if (sval > 0)
        printf("SVAL %d\n", sval);

    sem_post(&cycles_sem);
}
      
#endif
