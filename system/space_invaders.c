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
#include <unistd.h>
#include <SDL2/SDL.h>
#include "cpu/z80.h"
#include "space_invaders.h"

/* Space Invaders runs on z80 CPU, so let's instanciate its state struct */
z80_state_t *z80_state;

/* shift values for handling a custom behaviour of */
/* Space Invaders bit-shifting chip                */
uint16_t shift0, shift1, shift_offset;

/* window we'll be rendering to */
SDL_Window *window = NULL;

/* surface contained by the window */
SDL_Surface *screenSurface = NULL;

/* SDL keyboard state reference  */
const Uint8 *kb_state; 

/* set this to 1 if want to stop execution */
char quit = 0;

uint16_t latest_interrupt = 0xD7;

/* timer handler semaphore */
char timer_sem = 0;
char timer_triggered = 0;

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
    uint16_t word;

    switch(port)
    {
        case 1: /* we must act as the hardware listening to the port 1 
                   and set a register accordingly to the above schema  */

                /* e.g state->a = 0x08 if P1 shoot is pressed */
                z80_state->a = 0x01;

                /* refresh keyboard state */
                SDL_PumpEvents();

                /* check if Q is pressed */
                if (kb_state[SDL_SCANCODE_Q])
                {
                    quit = 1;
                    return;
                }

                /* UP pressed? +1 coin */
                if (kb_state[SDL_SCANCODE_UP])
                    z80_state->a = 0;     

                /* 2 pressed? 2 players start */
                if (kb_state[SDL_SCANCODE_2])
                    z80_state->a |= 0x02;                 

                /* 1 pressed? 1 player start */
                if (kb_state[SDL_SCANCODE_1])
                    z80_state->a |= 0x04;                 

                /* space pressed? fire */
                if (kb_state[SDL_SCANCODE_SPACE])
                    z80_state->a |= 0x10;                 

                /* left pressed? guess what */
                if (kb_state[SDL_SCANCODE_LEFT])
                    z80_state->a |= 0x20;                 

                /* right pressed? guess what */
                if (kb_state[SDL_SCANCODE_RIGHT])
                    z80_state->a |= 0x40;                 

                break;
       
        case 2: z80_state->a = 0x03;
                break;

        case 3: word = (shift1 << 8) | shift0; 
                z80_state->a = (uint8_t)((word >> (8 - shift_offset)) & 0xff);
                break;

        default: printf("UNKNOWN IN PORT\n"); sleep(5);
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
void space_invaders_out(uint8_t port)
{
    switch(port)
    {
        case 2: shift_offset = z80_state->a; 
                break;

        case 4: shift0 = shift1;
                shift1 = z80_state->a;
                break;
    }

    return;
}

/* callback for timer events (120 timer per second) */
void space_invaders_timer_handler(int sig, siginfo_t *si, void *uc)
{
    timer_triggered = 1;
}


/* called to synchronize video refresh rate */
void space_invaders_video_interrupt()
{
    int x, y, ib, i;
    uint8_t b, c, *p;
  
    if (timer_sem)
        return;

    timer_sem = 1;

    /* refresh keyboard state */   
    SDL_PumpEvents();

    /* check if Q is pressed */
    if (kb_state[SDL_SCANCODE_Q]) 
    {
        quit = 1;
        return;
    }        
 
    /* execute the RST 2 instruction */
    if (z80_state->int_enable)
    {
        if (latest_interrupt == 0xD7)
        {
            z80_execute(0xCF);
            latest_interrupt = 0xCF;
        }
        else
        {
            z80_execute(0xD7);
            latest_interrupt = 0xD7;
        }
    }

    /* refresh video! remember... it's 90 degrees rotated */
    for (x=0; x<256; x+=8)
    {
        for (y=0; y<224; y++)
        { 
            /* calc index of video memory */
            i = ((y * 256) + x) / 8;

            /* get byte from video RAM of the game */
            b = z80_state->memory[0x2400 + i];

            /* calc base index of SDL pixels buffer */
            i = (((255 - x) * 224) + y);

            /* loop over each bit of the byte */
            for (ib=0; ib<8; ib++)
            {
                /* get black or white pixel */
                c = b & 0x01;

                /* update SDL pixels buffer */
                p = (uint8_t *) screenSurface->pixels + 
                                (i * 4) - ((ib * 4) * 224);

                *p = c ? 0xFF : 0x00;
                *(p + 1) = c ? 0xFF : 0x00;
                *(p + 2) = c ? 0xFF : 0x00;
                *(p + 3) = 0xFF;

                /* shift it */
                b = b >> 1;
            }
        }
    }

    /* Update the surface */
    SDL_UpdateWindowSurface(window);

    timer_sem = 0;
}



/* entry point for Space Invaders emulate */
void space_invaders_start(uint8_t *rom, size_t size)
{
    uint8_t  op, port;
    struct itimerspec timer;
    struct sigevent   te;
    struct sigaction  sa;
    timer_t           timer_id = 0;

    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO) < 0 )
    {
        printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
        return;
    }

    /* sanity check */
    if (size > Z80_MAX_MEMORY)
    {
        printf("this ROM can't fit into memory\n");
        return;
    }

    window = SDL_CreateWindow("Emu Pizza - Space Invaders",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              224, 256, 
                              SDL_WINDOW_SHOWN);

    // Get window surface
    screenSurface = SDL_GetWindowSurface(window);

    /* get keyboard state reference */
    kb_state = SDL_GetKeyboardState(NULL);

    /* init 8080 */
    z80_state = z80_init(); 

    /* load ROM at 0x0000 address of system memory */
    memcpy(z80_state->memory, rom, size);

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

    /* initialize 120 hits per seconds timer (twice per drawn frame) */
    timer.it_value.tv_sec = 1;
    timer.it_value.tv_nsec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_nsec = 1000000000 / 120;
    
    /* start timer */
    timer_settime(timer_id, 0, &timer, NULL);

    /* running stuff! */
    while (!quit)
    {
        /* get op */
        op   = z80_state->memory[z80_state->pc];

        /* create a branch for every OP to override */
        switch (op)
        {
            /* IN     */
            case 0xDB: port = z80_state->memory[z80_state->pc + 1];
                       space_invaders_in(port);
                       break;

            /* OUT    */
            case 0xD3: port  = z80_state->memory[z80_state->pc + 1];
                       space_invaders_out(port);
                       break;
        }

        /* IN and OUT are = NOP in z80 stack */
        if (z80_execute(op))
        {
            quit = 1;

            break;
        }

        /* interrupts to handle? */
        if (timer_triggered)
        {
            space_invaders_video_interrupt();
            timer_triggered = 0;
        }
    }

    /* stop timer */
    timer_delete(timer_id);

    return; 
}
