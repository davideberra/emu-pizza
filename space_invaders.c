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

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "i8080.h"
#include "space_invaders.h"

/* Space Invaders runs on i8080 CPU, so let's instanciate its state struct */
i8080_state_t *i8080_state;

/* shift values for handling a custom behaviour of Space Invaders bit-shifting chip */
uint16_t shift0, shift1, shift_offset;


/*  
    handler for IN operation 

    Read 1    
    BIT 0   coin (0 when active)    
        1   P2 start button    
        2   P1 start button    
        3   ?    
        4   P1 shoot button    
        5   P1 joystick left    
        6   P1 joystick right    
        7   ?

    Read 2    
    BIT 0,1 dipswitch number of lives (0:3,1:4,2:5,3:6)    
        2   tilt 'button'    
        3   dipswitch bonus life at 1:1000,0:1500    
        4   P2 shoot button    
        5   P2 joystick left    
        6   P2 joystick right    
        7   dipswitch coin info 1:off,0:on    

    Read 3      shift register result    

*/
void space_invaders_in(uint8_t port)
{
    uint8_t  byte;
    uint16_t word;

    switch(port)
    {
        case 1: printf("IN 1\n");

                /* we must act as the hardware listening to the port 1 
                   and set a register accordingly to the above schema  */

                /* e.g state->a = 0x08 if P1 shoot is pressed */
                break;
       
        case 2: printf("IN 2\n");
                break;

        case 3: word = (shift1 << 8) | shift0; 
                i8080_state->a = (uint8_t) ((word >> (8 - shift_offset)) & 0xff);
                break;
    }

    return;
}

/* 
    handler for OUT operation 

    Write 2     shift register result offset (bits 0,1,2)    
    Write 3     sound related    
    Write 4     fill shift register    
    Write 5     sound related    
    Write 6     strange 'debug' port? eg. it writes to this port when    
            it writes text to the screen (0=a,1=b,2=c, etc)    

    (write ports 3,5,6 can be left unemulated, read port 1=$01 and 2=$00    
    will make the game run, but but only in attract mode)    

*/
void space_invaders_out(uint8_t port, uint8_t value)
{
    switch(port)
    {
        case 2: shift_offset = value & 0x07;
                break;

        case 4: shift0 = shift1;
                shift1 = value;
                break;
    }

    return;
}


/* called to synchronize video refresh rate */
void space_invaders_timer_handler(int sig, siginfo_t *si, void *uc)
{
    /* execute the RST 2 instruction */
    i8080_execute(0xD7);
}

/* entry point for Space Invaders emulate */
void space_invaders_start(uint8_t *rom, size_t size)
{
    int i;
    uint16_t addr;
    uint8_t  op, port, value;
    struct itimerspec timer;
    struct sigevent   te;
    struct sigaction  sa;
    timer_t           timer_id = 0;

    /* sanity check */
    if (size > I8080_MAX_MEMORY)
    {
        printf("this ROM can't fit into memory\n");
        return;
    }

    /* init 8080 */
    i8080_state = i8080_init(); 

    /* load ROM at 0x0000 address of system memory */
    memcpy(i8080_state->memory, rom, size);

    /* prepare timer to emulate video refresh interrupts */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = space_invaders_timer_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGRTMIN, &sa, NULL) == -1)
        return;
    bzero(&te, sizeof(struct sigevent));

    /* set and enable alarm */
    te.sigev_notify = SIGEV_SIGNAL;
    te.sigev_signo = SIGRTMIN;
    te.sigev_value.sival_ptr = &timer_id; 
    timer_create(CLOCK_REALTIME, &te, &timer_id);

    /* initialize 1 sec timer */
    timer.it_value.tv_sec = 1;
    timer.it_value.tv_nsec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_nsec = 500000000;
    
    /* start timer */
    timer_settime(timer_id, 0, &timer, NULL);

    /* running stuff! */
    for (;;)
    {
        i8080_disassemble(i8080_state->memory, i8080_state->pc);

        /* get op */
        op   = i8080_state->memory[i8080_state->pc];

        /* create a branch for every OP to override */
        switch (op)
        {
            /* IN     */
            case 0xDB: port = i8080_state->memory[i8080_state->pc + 1];
                       space_invaders_in(port);
                       break;

            /* OUT    */
            case 0xD3: port  = i8080_state->memory[i8080_state->pc + 1];
                       value = i8080_state->memory[i8080_state->pc + 2];
                       space_invaders_out(port, value);
                       break;
        }

        /* IN and OUT are = NOP in i8080 stack */
        if (i8080_run())
            break;
    }

    /* stop timer */
    timer_delete(timer_id);

    return; 
}
