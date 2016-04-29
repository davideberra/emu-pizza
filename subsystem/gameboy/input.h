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

#ifndef __INPUT__
#define __INPUT__

#include "subsystem/gameboy/globals.h"
#include <SDL2/SDL.h>

/* SDL keyboard state reference  */
const Uint8 *kb_state;

uint8_t input_init()
{
    /* get keyboard state reference */
    kb_state = SDL_GetKeyboardState(NULL);

    return 0;
}

uint8_t input_get_keys(uint8_t line)
{
    uint8_t v = line | 0x0f;

    /* refresh keyboard state */
    SDL_PumpEvents();

    /* check if Q is pressed */
    if (kb_state[SDL_SCANCODE_Q])
    {
        quit = 1;
        return 0;
    }

    if (kb_state[SDL_SCANCODE_W])
        gpu_window ^= 0x01;

    if (kb_state[SDL_SCANCODE_L])
        log_active ^= 0x01;

    if ((line & 0x30) == 0x20)
    { 
        /* RIGHT pressed? */
        if (kb_state[SDL_SCANCODE_RIGHT])
            v ^= 0x01;

        /* LEFT pressed? */
        if (kb_state[SDL_SCANCODE_LEFT])
            v ^= 0x02;

        /* UP pressed?   */
        if (kb_state[SDL_SCANCODE_UP])
            v ^= 0x04;

        /* DOWN pressed? */
        if (kb_state[SDL_SCANCODE_DOWN])
        {
            printf("PRESSATO DOWN\n");
            v ^= 0x08;
        }
    }

    if ((line & 0x30) == 0x10)
    {
        /* B pressed?      */
        if (kb_state[SDL_SCANCODE_Z])
            v ^= 0x01;

        /* A pressed?      */
        if (kb_state[SDL_SCANCODE_X])
            v ^= 0x02;

        /* SELECT pressed? */
        if (kb_state[SDL_SCANCODE_SPACE])
            v ^= 0x04;

        /* START pressed?  */
        if (kb_state[SDL_SCANCODE_RETURN])
            v ^= 0x08;
    }

    return (v | 0xc0);
}

#endif
