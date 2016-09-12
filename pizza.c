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

#include <SDL2/SDL.h>
#include <stdio.h>

#include "cartridge.h"
#include "gameboy.h"
#include "global.h"
#include "gpu.h"
#include "input.h"
#include "sound.h"

/* proto */
void cb();
void *start_thread(void *args);

/* frame buffer pointer */
uint16_t *fb;

/* magnify rate */
float magnify_rate = 3.f;

/* emulator thread */
pthread_t thread;

/* SDL video stuff */
SDL_Window *window;
SDL_Surface *screenSurface;
SDL_Surface *windowSurface;


int main(int argc, char **argv)
{
    /* SDL variables */
    SDL_Event e;
    SDL_AudioSpec desired;
    SDL_AudioSpec obtained;

    /* init global variables */
    global_init();

    /* first, load cartridge */
    char ret = cartridge_load(argv[1]);

    if (ret != 0)
        return 1;

    /* initialize SDL video */
    if (SDL_Init(SDL_INIT_VIDEO) < 0 )
    {
        printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
        return 1;
    }

    window = SDL_CreateWindow("Emu Pizza - Gameboy",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              160 * magnify_rate, 144 * magnify_rate,
                              SDL_WINDOW_SHOWN);

    /* get window surface */
    windowSurface = SDL_GetWindowSurface(window);
    screenSurface = SDL_ConvertSurfaceFormat(windowSurface,
                                             SDL_PIXELFORMAT_RGB565, 
                                             0);

    /* initialize SDL audio */
    SDL_Init(SDL_INIT_AUDIO);
    desired.freq = SOUND_FREQ;
    desired.samples = SOUND_SAMPLES;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.callback = sound_read_buffer;
    desired.userdata = NULL;

    /* Open audio */
    if (SDL_OpenAudio(&desired, &obtained) == 0)
        SDL_PauseAudio(0);
    else
    {
        printf("Cannot open audio device!!\n");
        return 1;
    }

    /* init GPU */
    gpu_init(&cb);

    /* get frame buffer reference */
    fb = gpu_get_frame_buffer();    

    /* start thread! */
    pthread_create(&thread, NULL, start_thread, NULL);

    /* loop forever */
    while (!global_quit)
    {
        /* aaaaaaaaaaaaaand finally, check for SDL events */

        /* SDL_WaitEvent should be better but somehow, */ 
        /* it interfer with my cycles timer            */
        if (SDL_PollEvent(&e) == 0)
        {
            usleep(100000);
            continue;
        }
        
        switch (e.type)
        {
            case SDL_QUIT:
                global_quit = 1;
                break;

            case SDL_KEYDOWN:
                switch (e.key.keysym.sym)
                {
                    case (SDLK_q): global_quit = 1; break;
                    case (SDLK_d): global_debug ^= 0x01; break;
                    case (SDLK_w): global_window ^= 0x01; break;
                    case (SDLK_p): gameboy_set_pause(global_pause ^ 0x01); 
                                   break;
                    case (SDLK_m): mmu_dump_all(); break;
                    case (SDLK_SPACE):  input_set_key_select(1); break;
                    case (SDLK_RETURN): input_set_key_start(1); break;
                    case (SDLK_UP):     input_set_key_up(1);    break;
                    case (SDLK_DOWN):   input_set_key_down(1);  break;
                    case (SDLK_RIGHT):  input_set_key_right(1); break;
                    case (SDLK_LEFT):   input_set_key_left(1);  break;
                    case (SDLK_z):      input_set_key_b(1);     break;
                    case (SDLK_x):      input_set_key_a(1);     break;
                }
                break;

            case SDL_KEYUP:
                switch (e.key.keysym.sym)
                {
                    case (SDLK_SPACE):  input_set_key_select(0); break;
                    case (SDLK_RETURN): input_set_key_start(0);  break;
                    case (SDLK_UP):     input_set_key_up(0);     break;
                    case (SDLK_DOWN):   input_set_key_down(0);   break;
                    case (SDLK_RIGHT):  input_set_key_right(0);  break;
                    case (SDLK_LEFT):   input_set_key_left(0);   break;
                    case (SDLK_z):      input_set_key_b(0);      break;
                    case (SDLK_x):      input_set_key_a(0);      break;
                }
                break;
        }
    }

    /* join emulation thread */   
    pthread_join(thread, NULL);
 
    return 0;
}

void *start_thread(void *args)
{
    /* run until break or global_quit is set */
    gameboy_run();

    /* tell main thread it's over */
    global_quit = 1;
}

void cb()
{
    uint16_t *pixel = screenSurface->pixels;

    /* magnify! */
    if (magnify_rate > 1)
    {
        int x,y,p;
        float px, py = 0;

        uint16_t *line = malloc(sizeof(uint16_t) * 160 * magnify_rate);

        for (y=0; y<144; y++)
        {
            px = 0;

            for (x=0; x<160; x++)
            {
                for (; px<magnify_rate; px++)
                    line[(int) (px + (x * magnify_rate))] =
                        fb[x + (y * 160)];

                px -= magnify_rate;
            }

            for (; py<magnify_rate; py++)
                memcpy(&pixel[(int) (((y * magnify_rate) + py) *
                           160 * magnify_rate)],
                       line, sizeof(uint16_t) * 160 * magnify_rate);

            py -= magnify_rate;
        }

        free(line);
    }
    else
    {
        /* just copy GPU frame buffer into SDL frame buffer */
        memcpy(pixel, fb, 160 * 144 * sizeof(uint16_t));
    }

    SDL_ConvertPixels(screenSurface->w, screenSurface->h,
                      screenSurface->format->format,
                      screenSurface->pixels, screenSurface->pitch,
                      SDL_PIXELFORMAT_ARGB8888,
                      windowSurface->pixels, windowSurface->pitch);
                       
    /* Update the surface */
    SDL_UpdateWindowSurface(window);
}
