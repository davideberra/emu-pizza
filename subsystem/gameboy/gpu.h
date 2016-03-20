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

#ifndef __GPU__
#define __GPU__

#include <SDL2/SDL.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include "subsystem/gameboy/globals.h"
#include "subsystem/gameboy/mmu_hdr.h"
#include "subsystem/gameboy/interrupts_hdr.h"

/* Gameboy OAM 4 bytes data */
typedef struct gpu_oam_s
{
    uint8_t y;
    uint8_t x;
    uint8_t pattern;

    uint8_t spare:4;
    uint8_t palette:1;
    uint8_t x_flip:1;
    uint8_t y_flip:1;
    uint8_t priority:1;

} gpu_oam_t;

/* window we'll be rendering to */
static SDL_Window *window = NULL;

/* surface contained by the window */
static SDL_Surface *screenSurface = NULL;
// static SDL_Surface *windowSurface = NULL;

/* pointer to interrupt flags (handy) */
interrupts_flags_t *gpu_if;

/* timer */
struct itimerspec gpu_timer;
struct sigevent   te;
struct sigaction  sa;
timer_t           gpu_timer_id = 0;

/* semaphore */
sem_t             gpu_sem;

/* boolean for timer trigger */
char              gpu_timer_triggered = 0;

/* prototype for timer handler */
void gpu_timer_handler(int sig, siginfo_t *si, void *uc);



/* REMOVE ME */
uint32_t fn = 0;

/* init GPU states */
void static gpu_init()
{
    bzero(&gpu_state, sizeof(gpu_t));

    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO) < 0 )
    {
        printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
        return;
    }

    window = SDL_CreateWindow("Emu Pizza - Gameboy",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              160, 144,
                              SDL_WINDOW_SHOWN);

    /* get window surface */
    // windowSurface = SDL_GetWindowSurface(window);
    screenSurface = SDL_GetWindowSurface(window);

    /* create a new screen surface (fixed 160x144 pixels) */
/*    screenSurface = SDL_CreateRGBSurface(0, 160, 144, windowSurface->format->BitsPerPixel, 
                                         windowSurface->format->Rmask,
                                         windowSurface->format->Gmask,
                                         windowSurface->format->Bmask,
                                         windowSurface->format->Amask);
*/

    /* make gpu field points to the related memory area */
    gpu_state.lcd_ctrl   = mmu_addr(0xFF40);
    gpu_state.lcd_status = mmu_addr(0xFF41);
    gpu_state.scroll_y   = mmu_addr(0xFF42);
    gpu_state.scroll_x   = mmu_addr(0xFF43);
    gpu_state.window_y   = mmu_addr(0xFF4A);
    gpu_state.window_x   = mmu_addr(0xFF4B);
    gpu_state.ly         = mmu_addr(0xFF44);
    gpu_state.lyc        = mmu_addr(0xFF45);
    gpu_if               = mmu_addr(0xFF0F);

    /* init semaphore for 60hz sync */
    sem_init(&gpu_sem, 0, 0);

    /* prepare timer to emulate video refresh interrupts */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = gpu_timer_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGRTMIN, &sa, NULL) == -1)
        return;
    bzero(&te, sizeof(struct sigevent)); 

    /* set and enable alarm */
    te.sigev_notify = SIGEV_SIGNAL;
    te.sigev_signo = SIGRTMIN;
    te.sigev_value.sival_ptr = &gpu_timer_id;
    timer_create(CLOCK_REALTIME, &te, &gpu_timer_id); 

    /* initialize 60 hits per seconds timer */
    gpu_timer.it_value.tv_sec = 1;
    gpu_timer.it_value.tv_nsec = 0;
    gpu_timer.it_interval.tv_sec = 0;
    gpu_timer.it_interval.tv_nsec = 1000000000 / 60; 

    /* start timer */
    timer_settime(gpu_timer_id, 0, &gpu_timer, NULL);
}

/* push frame on screen */
void static gpu_draw_frame()
{
    uint16_t i, j, x, sx, y, sy;

    uint32_t *pixel = screenSurface->pixels;

    for (i=0; i<144; i++)
    {
        y = (*(gpu_state.scroll_y) + i) % 256;

        /* calc shifted y just once */
        sy = y << 8;
 
        x = *(gpu_state.scroll_x);

        /* goind over the end of the screen? split it in 2 */
        if (x > 256 - 160)
        {
            /* calc how many pixel to copy at first shot */
            sx = 256 - x;

            /* copy first part of the line */
            memcpy(pixel, &gpu_state.frame_buffer[x + sy], sx * 4);

            /* copy second part of the line */
            memcpy(&pixel[sx], &gpu_state.frame_buffer[sy], (160 - sx) * 4);
        }
        else
        {
            /* boom - copy entire line */
            memcpy(pixel, &gpu_state.frame_buffer[x + sy], 160 * 4);
        }

        /* make pixel advance */
        pixel += 160;
    }

    /* magnify surface */
//    SDL_BlitScaled(screenSurface, NULL, windowSurface, NULL);

    /* Update the surface */
    SDL_UpdateWindowSurface(window);

    /* wait for 60hz clock */
    if (gpu_timer_triggered == 0)
        sem_wait(&gpu_sem);

    gpu_timer_triggered = 0;

    fn++;
}

/* draw a tile in x,y coordinates */
void static __always_inline gpu_draw_tile(uint8_t line, uint16_t base_address, 
                                          int16_t tile_n,
                                          uint8_t frame_x, uint8_t frame_y,
                                          uint32_t palette[4], char solid)
{
    int i, p, y;

    /* get absolute address of tiles area */
    uint8_t *tiles = mmu_addr(base_address);

    /* first pixel on frame buffer position */
    uint32_t tile_pos_fb = (frame_y * 256) + frame_x;
    // uint32_t tile_fb = ((frame_y + line) * 256) + frame_x;

    /* walk through 8x8 pixels                */ 
    /* (2bit per pixel  -> 4 pixels per byte  */
    /*  2 bytes per row -> 16 byte per tile)  */
    for (p=0; p<16; p+=2)
    {
        // uint8_t tile_x = (p * 4) % 8;
        uint8_t tile_y = (p * 4) / 8;

        /* calc frame position buffer for 4 pixels */
        uint32_t pos_fb = (tile_pos_fb + (tile_y * 256)) % 65536; // + tile_x

//        p = (line % 8) * 2;

        /* calc tile pointer */
        int16_t tile_ptr = (tile_n * 16) + p;

        /* pixels are handled in a super shitty way */
        /* bit 0 of the pixel is taken from even position tile bytes */
        /* bit 1 of the pixel is taken from odd position tile bytes */

        uint8_t  pxa[8];

        for (y=0; y<8; y++)
        {
             uint8_t shft = (1 << y);

             pxa[y] = ((*(tiles + tile_ptr) & shft) ? 1 : 0) |
                      ((*(tiles + tile_ptr + 1) & shft) ? 2 : 0);
        }

        /* set 8 pixels (full tile line) */
        for (i=0; i<8; i++)
            gpu_state.frame_buffer[pos_fb + (7 - i)] = palette[pxa[i]];
    }
}

/* draw a sprite tile in x,y coordinates */
void static __always_inline gpu_draw_sprite_tile(gpu_oam_t *oam, uint8_t sprites_size)
{
    int p, x, y, i, pos;
    uint32_t *palette;
  
    /* get absolute address of tiles area */
    uint8_t *tiles = mmu_addr(0x8000);

    /* REMEMBER! position of sprites is relative to the visible screen area */
    /* ... and y is shifted by 16 pixels, x by 8                            */
    y = (oam->y - 16 + *(gpu_state.scroll_y)) % 256;
    x = (oam->x - 8 + *(gpu_state.scroll_x)) % 256;

    /* first pixel on frame buffer position */
    uint32_t tile_pos_fb = (y * 256) + x;

    /* choose palette */
    if (oam->palette)
        palette = gpu_state.obj_palette_1;
    else
        palette = gpu_state.obj_palette_0;

    /* walk through 8x8 pixels (2bit per pixel -> 4 pixels per byte) */
    for (p=0; p<16 * (sprites_size + 1); p+=2)
    {
        uint8_t tile_y = (p * 4) / 8;

        /* calc frame position buffer for 4 pixels */
        uint32_t pos_fb = (tile_pos_fb + (tile_y * 256)) % 65536; 

        /* calc tile pointer */
        int16_t tile_ptr = (oam->pattern * 16) + p;

        /* pixels are handled in a super shitty way */
        /* bit 0 of the pixel is taken from even position tile bytes */
        /* bit 1 of the pixel is taken from odd position tile bytes */

        uint8_t  pxa[8];

        for (y=0; y<8; y++)
        {
             uint8_t shft = (1 << y);

             pxa[y] = ((*(tiles + tile_ptr) & shft) ? 1 : 0) |
                      ((*(tiles + tile_ptr + 1) & shft) ? 2 : 0);
        }

        /* set 8 pixels (full tile line) */
        for (i=0; i<8; i++)
        {
            if (oam->x_flip)
                pos = pos_fb + i;
            else
                pos = pos_fb + (7 - i);

            /* dont draw 0-color pixels */
            if (pxa[i] != 0x00 && 
               (oam->priority == 0 || 
               (oam->priority == 1 && gpu_state.frame_buffer[pos] == palette[0x00]))) 
                gpu_state.frame_buffer[pos] = palette[pxa[i]];
        }
    }
}

/* update GPU frame buffer */
void static gpu_update_frame_buffer(uint8_t line)
{
    int i, x, y, z, xmin, xmax, ymin, ymax;
    uint8_t *tiles_map;
    uint16_t tiles_addr, tile_n; 
    uint16_t tile_pos_x, tile_pos_y;

    /* gotta show BG */
    if ((*gpu_state.lcd_ctrl).bg)
    {
        /* get tile map offset */
        tiles_map = mmu_addr((*gpu_state.lcd_ctrl).bg_tiles_map ? 0x9C00 : 0x9800);

        if ((*gpu_state.lcd_ctrl).bg_tiles)
             tiles_addr = 0x8000;
        else
             tiles_addr = 0x9000;

        /* calc tiles involved */
        xmin = *(gpu_state.scroll_x) / 8;
        xmax = xmin + 21;

        ymin = *(gpu_state.scroll_y) / 8;
        ymax = ymin + 19;

        for (y=ymin; y<ymax; y++)
        {
            for (x=xmin; x<xmax; x++)
            {
                /* calc the linear index of the tile */
                z = (y % 32) * 32 + (x % 32);

                /* calc coordinates from the index of the tile */
                tile_pos_x = (z % 32) * 8;
                tile_pos_y = (z / 32) * 8;

                /* calc tile number */
                if ((*gpu_state.lcd_ctrl).bg_tiles == 0)
                     tile_n = (int8_t) tiles_map[z]; 
                else
                     tile_n = (tiles_map[z] & 0x00ff);

                gpu_draw_tile(line, tiles_addr, tile_n, 
                              (uint8_t) tile_pos_x, (uint8_t) tile_pos_y, 
                              gpu_state.bg_palette, 1);
            }
        }
    }

//    return;

    /* gotta show sprites? */
    if ((*gpu_state.lcd_ctrl).sprites)
    {
        /* make it point to the first OAM object */
        gpu_oam_t *oam = (gpu_oam_t *) mmu_addr(0xFE00);

        for (i=0; i<40; i++)
        {
            if (oam[i].x != 0)
                gpu_draw_sprite_tile(&oam[i], (*gpu_state.lcd_ctrl).sprites_size); 
        }
    }

    if (gpu_window && (*gpu_state.lcd_ctrl).window)
    {
        /* gotta really draw a window? check if it is inside screen coordinates */
        if (*(gpu_state.window_y) >= 144 ||
            *(gpu_state.window_x) >= 160)
            return;

        /* get tile map offset */
        tiles_map = mmu_addr((*gpu_state.lcd_ctrl).window_tiles_map ? 0x9C00 : 0x9800);

        /* window tiles are in this fixed area */
        if ((*gpu_state.lcd_ctrl).bg_tiles)
             tiles_addr = 0x8000;
        else
             tiles_addr = 0x9000;

        for (z=0; z<1024; z++)
        {
            if ((*gpu_state.lcd_ctrl).bg_tiles == 0)
                 tile_n = (int8_t) tiles_map[z];
            else
                 tile_n = (tiles_map[z] & 0x00ff);

            tile_pos_x = (z % 32) * 8 + *(gpu_state.window_x) + *(gpu_state.scroll_x) - 7;
            tile_pos_y = (z / 32) * 8 + *(gpu_state.window_y) + *(gpu_state.scroll_y);

            /* gone over the screen visible X? */
//            if (tile_pos_x > *(gpu_state.scroll_x) + 160)
//                continue;

            /* gone over the screen visible Y? stop it */
            if (tile_pos_y > *(gpu_state.scroll_y) + 144)
                break;

            gpu_draw_tile(0, tiles_addr, tile_n, 
                          (uint8_t) tile_pos_x, (uint8_t) tile_pos_y,
                          gpu_state.bg_palette, 1);
        }
    }

}

/* update GPU internal state given CPU T-states */
void static __always_inline gpu_step(uint8_t t)
{
    char ly_changed = 0;
    char mode_changed = 0;

    /* update clock counter */
    gpu_state.clocks += t;

    switch((*gpu_state.lcd_status).mode)
    {
        /*
         * during HBLANK (CPU can access VRAM)
         */
        case 0: /* check if an HBLANK is complete (201 t-states) */
                if (gpu_state.clocks > 200)
                {
                    /*
                     * if current line > 142, enter mode 01 (VBLANK)
                     */
                    if (*gpu_state.ly > 142)
                    {
                        /* notify mode has changes */
                        mode_changed = 1;

                        (*gpu_state.lcd_status).mode = 0x01;

                        /* DRAW! TODO */
                        /* CHECK INTERRUPTS! TODO */

                        /* set VBLANK interrupt flag */
                        gpu_if->lcd_vblank = 1;

                        /* update frame in memory */
                        gpu_update_frame_buffer(0);

                        /* and finally push it on screen! */
                        gpu_draw_frame();
                    } 
                    else
                    {
                        /* notify mode has changed */
                        mode_changed = 1;

                        /* enter OAM mode */
                        (*gpu_state.lcd_status).mode = 0x02;
                    }

                    /* notify mode has changed */
                    ly_changed = 1;

                    /* inc current line */
                    (*gpu_state.ly)++;

                    /* reset clock counter */
                    gpu_state.clocks -= 201;
                }

                break;

        /*
         * during VBLANK (CPU can access VRAM)
         */
        case 1: /* check if an HBLANK is complete (460 t-states) */
                if (gpu_state.clocks > 459) 
                {
                    /* reset clock counter */
                    gpu_state.clocks -= 460;

                    /* notify ly has changed */
                    ly_changed = 1;

                    /* inc current line */
                    (*gpu_state.ly)++;

                    /* reached the bottom? */
                    if ((*gpu_state.ly) > 153)
                    {
                        /* go back to line 0 */
                        (*gpu_state.ly) = 0;

                        /* switch to OAM mode */
                        (*gpu_state.lcd_status).mode = 0x02;
                    }
                }

                break;

        /*
         * during OAM (LCD access FE00-FE90, so CPU cannot)
         */
        case 2: /* check if an OAM is complete (80 t-states) */
                if (gpu_state.clocks > 79)
                {
                    /* reset clock counter */
                    gpu_state.clocks -= 80;

                    /* notify mode has changed */
                    mode_changed = 1;

                    /* switch to VRAM mode */
                    (*gpu_state.lcd_status).mode = 0x03;
                }

                break;

        /*
         * during VRAM (LCD access both OAM and VRAM, so CPU cannot)
         */
        case 3: /* check if a VRAM read is complete (177 t-states) */
                if (gpu_state.clocks > 176)
                {
                    /* reset clock counter */
                    gpu_state.clocks -= 177;

                    /* notify mode has changed */
                    mode_changed = 1;

                    /* go back to HBLANK mode */
                    (*gpu_state.lcd_status).mode = 0x00;
                }

                break;

    }

    /* ly changed? is it the case to trig an interrupt? */
    if (ly_changed)
    {
        /* update prev line - IT MUST BE DONE EVERY LINE because */
        /* values like scroll x or scroll y could change         */
        /* during the draw of a single frame                     */
//        gpu_update_frame_buffer((*gpu_state.ly) - 1);
 
        /* check if we gotta trigger an interrupt */
        if ((*gpu_state.ly) == (*gpu_state.lyc))
        { 
            /* set lcd status flags indicating there's a concidence */
            (*gpu_state.lcd_status).ly_coincidence = 1;

            /* an interrupt is desiderable? */
            if ((*gpu_state.lcd_status).ir_ly_coincidence)
                gpu_if->lcd_ctrl = 1;
        }
        else
        {
            /* set lcd status flags indicating there's NOT a concidence */
            (*gpu_state.lcd_status).ly_coincidence = 0;
        }
    }

    /* mode changed? is is the case to trig an interrupt? */
    if (mode_changed)
    {
        if ((*gpu_state.lcd_status).mode == 0x00 &&
            (*gpu_state.lcd_status).ir_mode_00)
            gpu_if->lcd_ctrl = 1;
        else if ((*gpu_state.lcd_status).mode == 0x01 &&
                 (*gpu_state.lcd_status).ir_mode_01)
            gpu_if->lcd_ctrl = 1;
        else if ((*gpu_state.lcd_status).mode == 0x02 &&
                 (*gpu_state.lcd_status).ir_mode_10)
            gpu_if->lcd_ctrl = 1;
    }
}

/* callback for timer events (60 timer per second) */
void gpu_timer_handler(int sig, siginfo_t *si, void *uc)
{
    gpu_timer_triggered = 1;

    /* unlock semaphore */
    sem_post(&gpu_sem);
}

#endif 
