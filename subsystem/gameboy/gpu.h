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

#include "subsystem/gameboy/mmu_hdr.h"
#include "subsystem/gameboy/interrupts_hdr.h"
#include <SDL2/SDL.h>

/* Gameboy OAM 4 bytes data */
typedef struct gpu_oam_s
{
    uint8_t y;
    uint8_t x;
    uint8_t pattern;

    uint8_t priority:1;
    uint8_t y_flip:1;
    uint8_t x_flip:1;
    uint8_t palette:1;
    uint8_t spare:4;

} gpu_oam_t;

/* window we'll be rendering to */
static SDL_Window *window = NULL;

/* surface contained by the window */
static SDL_Surface *screenSurface = NULL;

/* pointer to interrupt flags (handy) */
interrupts_flags_t *gpu_if;

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

    // Get window surface
    screenSurface = SDL_GetWindowSurface(window);

    /* make gpu field points to the related memory area */
    gpu_state.lcd_ctrl   = mmu_addr(0xFF40);
    gpu_state.lcd_status = mmu_addr(0xFF41);
    gpu_state.scroll_y   = mmu_addr(0xFF42);
    gpu_state.scroll_x   = mmu_addr(0xFF43);
    gpu_state.curline    = mmu_addr(0xFF44);
    gpu_if               = mmu_addr(0xFF0F);
}

/* push frame on screen */
void static gpu_draw_frame()
{
    uint16_t i, j, x, y;

    uint32_t *pixel = screenSurface->pixels;

    y = *(gpu_state.scroll_y);
 
    for (i=0; i<144; i++)
    {
        y = (y + 1) % 256;
 
        x = *(gpu_state.scroll_x);

        for (j=0; j<160; j++)
        {
            x = (x + 1) % 256;

            *pixel = 0x00000000 | 
                     (gpu_state.frame_buffer[x + (y << 8)] << 8);

            pixel++;
        }
    } 


/*    for (y=0; y<256; y++)
    {
        for (x=0; x<256; x++)
        {
            *pixel = 0x00000000 |
                     (gpu_state.frame_buffer[x + (y << 8)] << 8);

            pixel++;
        }
    } */


    /* Update the surface */
    SDL_UpdateWindowSurface(window);

    usleep(1000000 / 60);
}

/* draw a tile in x,y coordinates */
void static __always_inline gpu_draw_tile(uint16_t base_address, int16_t tile_n,
                                          uint8_t frame_x, uint8_t frame_y,
                                          uint8_t palette[4])
{
    int p, y;

    /* get absolute address of tiles area */
    uint8_t *tiles = mmu_addr(base_address);

    /* first pixel on frame buffer position */
    uint32_t tile_pos_fb = (frame_y * 256) + frame_x;

    /* walk through 8x8 pixels (2bit per pixel -> 4 pixels per byte) */
    for (p=0; p<16; p+=2)
    {
        // uint8_t tile_x = (p * 4) % 8;
        uint8_t tile_y = (p * 4) / 8;

        /* calc frame position buffer for 4 pixels */
        uint32_t pos_fb = (tile_pos_fb + (tile_y * 256)) % 65536; // + tile_x

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
        gpu_state.frame_buffer[pos_fb]     = palette[pxa[7]];
        gpu_state.frame_buffer[pos_fb + 1] = palette[pxa[6]];
        gpu_state.frame_buffer[pos_fb + 2] = palette[pxa[5]];
        gpu_state.frame_buffer[pos_fb + 3] = palette[pxa[4]];
        gpu_state.frame_buffer[pos_fb + 4] = palette[pxa[3]];
        gpu_state.frame_buffer[pos_fb + 5] = palette[pxa[2]];
        gpu_state.frame_buffer[pos_fb + 6] = palette[pxa[1]];
        gpu_state.frame_buffer[pos_fb + 7] = palette[pxa[0]];
    }

}

/* draw a sprite tile in x,y coordinates */
void static __always_inline gpu_draw_sprite_tile(uint16_t base_address, int16_t tile_n,
                                                 uint8_t frame_x, uint8_t frame_y,
                                                 uint8_t palette[4])
{
    int p, y, i;
    
    /* get absolute address of tiles area */
    uint8_t *tiles = mmu_addr(base_address);

    /* first pixel on frame buffer position */
    uint32_t tile_pos_fb = (frame_y * 256) + frame_x;

    /* walk through 8x8 pixels (2bit per pixel -> 4 pixels per byte) */
    for (p=0; p<16; p+=2)
    {
        // uint8_t tile_x = (p * 4) % 8;
        uint8_t tile_y = (p * 4) / 8;

        /* calc frame position buffer for 4 pixels */
        uint32_t pos_fb = (tile_pos_fb + (tile_y * 256)) % 65536; // + tile_x

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
        {
            /* dont draw 0-color pixels */
            if (pxa[i] != 0x00)
                gpu_state.frame_buffer[pos_fb + (7 - i)] = palette[pxa[i]];
        }
    }
}

/* update GPU frame buffer */
void static gpu_update_frame_buffer()
{
    int i, z;

    /* gotta show BG */
    if ((*gpu_state.lcd_ctrl).bg)
    {
        /* get tile map offset */
        uint8_t *tiles_map = mmu_addr((*gpu_state.lcd_ctrl).bg_tiles_map ? 0x9C00 : 
                                                                           0x9800);
        uint16_t tiles_addr; 

        if ((*gpu_state.lcd_ctrl).bg_tiles)
             tiles_addr = 0x8000;
        else
             tiles_addr = 0x9000;

        for (z=0; z<1024; z++)
        {
            int16_t   tile_n;

            if ((*gpu_state.lcd_ctrl).bg_tiles == 0)
                 tile_n = (int8_t) tiles_map[z]; 
            else
                 tile_n = (tiles_map[z] & 0x00ff);

            uint8_t  tile_pos_x = (z % 32) * 8;
            uint8_t  tile_pos_y = (z / 32) * 8;

            gpu_draw_tile(tiles_addr, tile_n, tile_pos_x, tile_pos_y, 
                          gpu_state.bg_palette);
        }
    }

    /* gotta show sprites? */
    if ((*gpu_state.lcd_ctrl).sprites)
    {
        /* make it point to the first OAM object */
        gpu_oam_t *oam = (gpu_oam_t *) mmu_addr(0xFE00);

        for (i=0; i<40; i++)
        {
            if (oam->x != 0)
            {
/*                printf("SPRITE. PATTERN: %d - CAST: %d - X: %d - Y: %d\n",
                       oam->pattern, (int16_t) oam->pattern, oam->x, oam->y); */

                if (oam->palette) 
                    gpu_draw_sprite_tile(0x8000, oam->pattern, oam->x - 8, oam->y - 16,
                                  gpu_state.obj_palette_1);
                else
                    gpu_draw_sprite_tile(0x8000, oam->pattern, oam->x - 8, oam->y - 16,
                                  gpu_state.obj_palette_0);
                    
            }

            /* point to the next object */
            oam++;
        }
    }

/*    if ((*gpu_state.lcd_ctrl).window) 
    {
        printf("LA WINDOW VA MOSTRATA\n");
    }
    else
        printf("NIENTE WINDOW\n");
*/

}

/* update GPU internal state given CPU T-states */
void static __always_inline gpu_step(uint8_t t)
{
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
                    if (*gpu_state.curline > 142)
                    {
                        (*gpu_state.lcd_status).mode = 0x01;

                        /* DRAW! TODO */
                        /* CHECK INTERRUPTS! TODO */

                        /* set VBLANK interrupt flag */
                        gpu_if->lcd_vblank = 1;

                        /* update frame in memory */
                        gpu_update_frame_buffer();

                        /* and finally push it on screen! */
                        gpu_draw_frame();
                    } 
                    else
                    {
                        /* enter OAM mode */
                        (*gpu_state.lcd_status).mode = 0x02;
                    }

                    /* inc current line */
                    (*gpu_state.curline)++;

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

                    /* inc current line */
                    (*gpu_state.curline)++;

                    /* reached the bottom? */
                    if ((*gpu_state.curline) > 153)
                    {
                        /* go back to line 0 */
                        (*gpu_state.curline) = 0;

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

                    /* go back to HBLANK mode */
                    (*gpu_state.lcd_status).mode = 0x00;
                }

                break;

    }
}

#endif 
