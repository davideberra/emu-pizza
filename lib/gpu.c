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

#include "global.h"
#include "interrupt.h"
#include "mmu.h"
#include "gpu.h"

#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* Gameboy OAM 4 bytes data */
typedef struct gpu_oam_s
{
    uint8_t y;
    uint8_t x;
    uint8_t pattern;

    uint8_t palette_cgb:3;
    uint8_t vram_bank:1;
    uint8_t palette:1;
    uint8_t x_flip:1;
    uint8_t y_flip:1;
    uint8_t priority:1;

} gpu_oam_t;

/* Gameboy Color additional tile attributes */
typedef struct gpu_cgb_bg_tile_s
{
    uint8_t palette:3;
    uint8_t vram_bank:1;
    uint8_t spare:1;
    uint8_t x_flip:1;
    uint8_t y_flip:1;
    uint8_t priority:1;

} gpu_cgb_bg_tile_t;

/* ordered sprite list */
typedef struct oam_list_s
{
    int idx;
    struct oam_list_s *next;
} oam_list_t;

/* pointer to interrupt flags (handy) */
interrupts_flags_t *gpu_if;

/* internal functions prototypes */
void gpu_draw_sprite_line(gpu_oam_t *oam, 
                          uint8_t sprites_size,
                          uint8_t line);
void gpu_draw_window_line(int tile_idx, uint8_t frame_x,
                          uint8_t frame_y, uint8_t line);

/* as the name says.... */
int gpu_magnify_rate = 3;

/* total cycles */
uint32_t gpu_total_cycles;

/* 2 bit to 8 bit color lookup */
static uint16_t gpu_color_lookup[] = { 0xFFFF, 0xAD55, 0x52AA, 0x0000 };

/* function to call when frame is ready */
gpu_frame_ready_cb_t gpu_frame_ready_cb;

/* global state of GPU */
gpu_t gpu;


/* init GPU states */
void gpu_init(gpu_frame_ready_cb_t cb)
{
    /* reset gpu structure */
    bzero(&gpu, sizeof(gpu_t));

    /* make gpu field points to the related memory area */
    gpu.lcd_ctrl   = mmu_addr(0xFF40);
    gpu.lcd_status = mmu_addr(0xFF41);
    gpu.scroll_y   = mmu_addr(0xFF42);
    gpu.scroll_x   = mmu_addr(0xFF43);
    gpu.window_y   = mmu_addr(0xFF4A);
    gpu.window_x   = mmu_addr(0xFF4B);
    gpu.ly         = mmu_addr(0xFF44);
    gpu.lyc        = mmu_addr(0xFF45);
    gpu_if         = mmu_addr(0xFF0F);
   
    /* init counters */ 
    gpu.clocks = 0;
    gpu_total_cycles = 0;

    /* init palette */
    memcpy(gpu.bg_palette, gpu_color_lookup, sizeof(uint16_t) * 4);
    memcpy(gpu.obj_palette_0, gpu_color_lookup, sizeof(uint16_t) * 4);
    memcpy(gpu.obj_palette_1, gpu_color_lookup, sizeof(uint16_t) * 4);

    /* set callback */
    gpu_frame_ready_cb = cb;
}

/* turn on/off lcd */
void gpu_toggle(uint8_t state)
{
    /* from off to on */
    if (state & 0x80)
    {
        /* LCD turned on */
        gpu.clocks = 0;
        *gpu.ly  = 0;
        (*gpu.lcd_status).mode = 0x00;
        (*gpu.lcd_status).ly_coincidence = 0x00;
    }
    else
    {
        /* LCD turned off - reset stuff */
        gpu.clocks = 0;
        *gpu.ly = 0;
    }
} 

/* push frame on screen */
void gpu_draw_frame()
{
    /* call the callback */
    if (gpu_frame_ready_cb)
        (*gpu_frame_ready_cb) ();

    /* reset priority matrix */
    bzero(gpu.priority, 160 * 144);

    return;
}

/* get pointer to frame buffer */
uint16_t *gpu_get_frame_buffer()
{
    return gpu.frame_buffer;
}

/* draw a single line */
void gpu_draw_line(uint8_t line)
{
    int i, t, y, px_start, px_drawn;
    uint8_t *tiles_map, tile_subline, palette_idx;
    uint16_t tiles_addr, tile_n, tile_idx, tile_line;
    uint16_t tile_y;
    
    /* avoid mess */
    if (line > 144)
        return;

    /* gotta show BG */
    if ((*gpu.lcd_ctrl).bg)
    {
        gpu_cgb_bg_tile_t *tiles_map_cgb = NULL;

        /* get tile map offset */
        /*tiles_map = mmu_addr((*gpu.lcd_ctrl).bg_tiles_map ? 
                             0x9C00 : 0x9800);

        if ((*gpu.lcd_ctrl).bg_tiles)
             tiles_addr = 0x8000;
        else
             tiles_addr = 0x9000; */

        uint8_t *tiles = NULL; 
        uint16_t *palette;

        if (global_cgb)
        {
            /* CGB tile map into VRAM0 */
            tiles_map = mmu_addr_vram0() + ((*gpu.lcd_ctrl).bg_tiles_map ?
                                  0x1C00 : 0x1800);

            /* additional attribute table is into VRAM1 */
            tiles_map_cgb = mmu_addr_vram1() + ((*gpu.lcd_ctrl).bg_tiles_map ?
                                  0x1C00 : 0x1800);
        }
        else
        {
            /* get tile map offset */
            tiles_map = mmu_addr((*gpu.lcd_ctrl).bg_tiles_map ? 
                                 0x9C00 : 0x9800);

            if ((*gpu.lcd_ctrl).bg_tiles)
                 tiles_addr = 0x8000;
            else
                 tiles_addr = 0x9000;

            /* get absolute address of tiles area */
            tiles = mmu_addr(tiles_addr);

            /* monochrome GB uses a single BG palette */
            palette = gpu.bg_palette; 
        }

        /* calc tile y */
        tile_y = (*(gpu.scroll_y) + line) & 0xFF;

        /* calc first tile idx */
        tile_idx = ((tile_y >> 3) * 32) + (*(gpu.scroll_x) / 8);

        /* tile line because if we reach the end of the line,   */
        /* we have to rewind to the first tile of the same line */     
        tile_line = ((tile_y >> 3) * 32); 
  
        /* calc first pixel of frame buffer of the current line */
        uint16_t pos_fb = line * 160;
 
        /* calc tile subline */
        tile_subline = tile_y % 8;

        /* walk through different tiles */
        for (t=0; t<21; t++)
        {
            /* resolv tile data memory area */ 
            if ((*gpu.lcd_ctrl).bg_tiles == 0)
                tile_n = (int8_t) tiles_map[tile_idx];
            else
                tile_n = (tiles_map[tile_idx] & 0x00FF);

            /* if color gameboy, resolv which palette is bound */
            if (global_cgb)
            {
                palette_idx = tiles_map_cgb[tile_idx].palette;

//                if (tile_idx > 0x00 && tile_idx < 0x20)
//                    printf("TILE IDX %02x - TILE N %02x - PALETTE %d\n", tile_idx, tile_n, palette_idx);

                /* get palette pointer to 4 (16bit) colors */
                palette = 
                    (uint16_t *) &gpu.cgb_palette_bg_rgb565[palette_idx * 4];

                if (tiles_map_cgb[tile_idx].vram_bank)
                    tiles = mmu_addr_vram1() +
                            ((*gpu.lcd_ctrl).bg_tiles ? 0x0000 : 0x1000);
                else
                    tiles = mmu_addr_vram0() +
                            ((*gpu.lcd_ctrl).bg_tiles ? 0x0000 : 0x1000);
            }

            /* calc tile data pointer */
            int16_t tile_ptr = (tile_n * 16) + (tile_subline * 2);

            /* pixels are handled in a super shitty way                  */
            /* bit 0 of the pixel is taken from even position tile bytes */
            /* bit 1 of the pixel is taken from odd position tile bytes  */

            uint8_t  pxa[8];
            uint8_t  shft;
            uint8_t  b1 = *(tiles + tile_ptr);
            uint8_t  b2 = *(tiles + tile_ptr + 1);

            for (y=0; y<8; y++)
            {
                 shft = (1 << y);

                 pxa[y] = ((b1 & shft) ? 1 : 0) |
                          ((b2 & shft) ? 2 : 0);
            }

            /* particular cases for first and last tile */ 
            /* (could be shown just a part)             */
            if (t == 0)
            {
                px_start = (*(gpu.scroll_x) % 8);

                px_drawn = 8 - px_start;

                /* set n pixels */
                for (i=0; i<px_drawn; i++)
                    gpu.frame_buffer[pos_fb + (px_drawn - i - 1)] = 
                        palette[pxa[i]];
            }
            else if (t == 20)
            {
                px_drawn = *(gpu.scroll_x) % 8;

                /* set n pixels */
                for (i=0; i<px_drawn; i++)
                    gpu.frame_buffer[pos_fb + (px_drawn - i - 1)] = 
                        palette[pxa[i + (8 - px_drawn)]];
            } 
            else
            {
                /* set 8 pixels */
                for (i=0; i<8; i++)
                    gpu.frame_buffer[pos_fb + (7 - i)] = palette[pxa[i]];

                px_drawn = 8;
            }

            /* go to the next tile and rewind in case we reached the 32th */
            tile_idx++;

            /* don't go to the next line, just rewind */
            if (tile_idx == (tile_line + 32))
                tile_idx = tile_line;

            /* go to the next block of 8 pixels of the frame buffer */
            pos_fb += px_drawn;
        }
    }

    /* gotta show sprites? */
    if ((*gpu.lcd_ctrl).sprites)
    {
        /* make it point to the first OAM object */
        gpu_oam_t *oam = (gpu_oam_t *) mmu_addr(0xFE00);

        /* calc sprite height */
        uint8_t h = ((*gpu.lcd_ctrl).sprites_size + 1) * 8;

        int sort[40];

        /* prepare sorted list of oams */        
        for (i=0; i<40; i++)
            sort[i] = -1;
        
        for (i=0; i<40; i++)
        {
            /* the sprite intersects the current line? */
            if (oam[i].x != 0 && oam[i].y != 0 &&
                oam[i].x < 168 && oam[i].y < 160 &&
                line < (oam[i].y + h - 16) &&
                line >= (oam[i].y - 16))
            {
                int j;

                /* find its position on sort array */
                for (j=0; j<40; j++)
                {
                    if (sort[j] == -1)
                    {
                        sort[j] = i;
                        break;
                    }

                    if ((oam[i].y < oam[sort[j]].y) ||
                        ((oam[i].y == oam[sort[j]].y) &&
                         (oam[i].x < oam[sort[j]].x)))
                    {
                        int z;

                        for (z=40; z>j; z--)
                            sort[z] = sort[z-1];

                        sort[j] = i;
                        break;
                    }
                } 
            }                  
        } 

        /* draw ordered sprite list */
        for (i=0; i<40 && sort[i] != -1; i++)
            gpu_draw_sprite_line(&oam[sort[i]], 
                                 (*gpu.lcd_ctrl).sprites_size, line);
        
    }

    /* wanna show window? */
    if (global_window && (*gpu.lcd_ctrl).window)
    {
        /* at least the current line is covering the window area? */
        if (line < *(gpu.window_y))
            return;
 
        int z, first_z;
        uint8_t tile_pos_x, tile_pos_y;

        /* gotta draw a window? check if it is inside screen coordinates */
        if (*(gpu.window_y) >= 144 ||
            *(gpu.window_x) >= 160)
            return; 

        if (global_cgb)
        {
            /* CGB tile map into VRAM0 */
            tiles_map = mmu_addr_vram0() + ((*gpu.lcd_ctrl).window_tiles_map ?
                                  0x1C00 : 0x1800);
        }
        else
        {
            /* get tile map offset */
            tiles_map = mmu_addr((*gpu.lcd_ctrl).window_tiles_map ?
                                 0x9C00 : 0x9800);
        }

        /* calc the first interesting tile */
        first_z = ((line - *(gpu.window_y)) >> 3) << 5;

        /* add X offset */
        first_z += (*(gpu.window_x) - 7) >> 3;

        for (z=first_z; z<first_z + 21; z++)
        {
            /* calc tile coordinates on frame buffer */
            tile_pos_x = ((z & 0x1F) << 3) + *(gpu.window_x) - 7;
            tile_pos_y = ((z >> 5) << 3) + *(gpu.window_y);

            /* gone over the current line? */
            if (tile_pos_y > line)
                break;

            if (tile_pos_y < (line - 7))
                continue;
                
            /* gone over the screen visible X? */
            /* being between last column and first one is valid */
            if (tile_pos_x >= 160 && tile_pos_x < 248)
                continue;

            /* gone over the screen visible Y? stop it */
            if (tile_pos_y >= 144)
                break;

            //printf("LINEA %d - TILE %d - TILE POS %d - WINDOW Y %d\n", 
            //       line, z, tile_pos_y, *(gpu.window_y));

/*            if ((*gpu.lcd_ctrl).bg_tiles == 0)
                 tile_n = (int8_t) tiles_map[z];
            else
                 tile_n = (tiles_map[z] & 0x00ff); */

            /* put tile on frame buffer */
            gpu_draw_window_line(z, (uint8_t) tile_pos_x, 
                                 (uint8_t) tile_pos_y, line);
        }

    }

         //                                                                                                                                           tiles_addr = 0x9000; */
         //
         //                                                                                                                                                       /* get absolute address of tiles area */
         //                                                                                                                                                                   // tiles = mmu_addr(tiles_addr);
         //
         //                                                                                                                                                                               /* monochrome GB uses a single BG palette */
         //                                                                                                                                                                                           // palette = gpu.bg_palette;
         //                                                                                                                                                                                                   }
         //
}



/* draw a tile in x,y coordinates */
void gpu_draw_window_line(int tile_idx, uint8_t frame_x, 
                          uint8_t frame_y, uint8_t line)
{
    int i, p, y, pos;
    int16_t tile_n;
    uint8_t *tiles_map;
    gpu_cgb_bg_tile_t *tiles_map_cgb = NULL;
    uint8_t *tiles;
    uint16_t *palette;

    if (global_cgb)
    {
        /* CGB tile map into VRAM0 */
        tiles_map = mmu_addr_vram0() + ((*gpu.lcd_ctrl).window_tiles_map ?
                              0x1C00 : 0x1800);

        /* additional attribute table is into VRAM1 */
        tiles_map_cgb = mmu_addr_vram1() + ((*gpu.lcd_ctrl).window_tiles_map ?
                              0x1C00 : 0x1800);

        /* get palette index */
        uint8_t palette_idx = tiles_map_cgb[tile_idx].palette;

        /* get palette pointer to 4 (16bit) colors */
        palette = (uint16_t *) &gpu.cgb_palette_bg_rgb565[palette_idx * 4];

        /* attribute table will tell us where is the tile */
        if (tiles_map_cgb[tile_idx].vram_bank)
            tiles = mmu_addr_vram1() + 
                    ((*gpu.lcd_ctrl).bg_tiles ? 0x0000 : 0x1000);
        else
            tiles = mmu_addr_vram0() + 
                    ((*gpu.lcd_ctrl).bg_tiles ? 0x0000 : 0x1000);
           
    }
    else
    {
        /* get tile map offset */
        tiles_map = mmu_addr((*gpu.lcd_ctrl).window_tiles_map ?
                             0x9C00 : 0x9800);

        /* get tile offset */
        if ((*gpu.lcd_ctrl).bg_tiles)
            tiles = mmu_addr(0x8000);
        else
            tiles = mmu_addr(0x9000);

        /* monochrome GB uses a single BG palette */
        palette = gpu.bg_palette;
    }

    /* obtain tile number */
    if ((*gpu.lcd_ctrl).bg_tiles == 0)
        tile_n = (int8_t) tiles_map[tile_idx];
    else
        tile_n = (tiles_map[tile_idx] & 0x00ff);

    /* calc vertical offset INSIDE the tile */
    p = (line - frame_y) * 2; 

    /* calc frame position buffer for 4 pixels */
    uint32_t pos_fb = (line * 160); //(tile_pos_fb + (tile_y * 160)) % (160 * 144); 

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
        /* over the last column? */
        uint8_t x = frame_x + (7 - i);

        if (x > 159)
            continue;

        /* calc position on frame buffer */
        pos = pos_fb + x; 

        /* can overwrite sprites? depends on pixel priority */
        if (gpu.priority[pos] == 0)
            gpu.frame_buffer[pos] = palette[pxa[i]];
    }
}

/* draw a sprite tile in x,y coordinates */
void gpu_draw_sprite_line(gpu_oam_t *oam, uint8_t sprites_size, uint8_t line)
{
    int p, x, y, i, j, pos, fb_x, off;
    uint8_t  sprite_bytes;
    int16_t  tile_ptr;
    uint16_t *palette;
    uint8_t *tiles;
   
    /* REMEMBER! position of sprites is relative to the visible screen area */
    /* ... and y is shifted by 16 pixels, x by 8                            */
    y = oam->y - 16;
    x = oam->x - 8;
  
    if (x < -7)
        return;

    /* first pixel on frame buffer position */
    uint32_t tile_pos_fb = (y * 160) + x;

    /* choose palette */
    if (global_cgb)
    {
         uint8_t palette_idx = oam->palette_cgb;

         /* get palette pointer to 4 (16bit) colors */
         palette = (uint16_t *) &gpu.cgb_palette_oam_rgb565[palette_idx * 4];
   
         /* tiles are into vram0 */
         tiles = mmu_addr_vram0();
    }
    else
    {
        /* tiles are int fixed 0x8000 address */
        tiles = mmu_addr(0x8000);

        if (oam->palette)
            palette = gpu.obj_palette_1;
        else
            palette = gpu.obj_palette_0;
    }

    /* calc sprite in byte */
    sprite_bytes = 16 * (sprites_size + 1);

    /* walk through 8x8 pixels (2bit per pixel -> 4 pixels per byte) */
    /* 1 line is 8 pixels -> 2 bytes per line                        */
    for (p=0; p<sprite_bytes; p+=2)
    {
        uint8_t tile_y = p / 2;

        if (tile_y + y != line)
            continue;

        /* calc frame position buffer for 4 pixels */
        uint32_t pos_fb = (tile_pos_fb + (tile_y * 160)) % 65536; 

        /* calc tile pointer */
        if (oam->y_flip)
             tile_ptr = (oam->pattern * 16) + (sprite_bytes - p - 2);
        else
             tile_ptr = (oam->pattern * 16) + p;

        /* pixels are handled in a super shitty way */
        /* bit 0 of the pixel is taken from even position tile bytes */
        /* bit 1 of the pixel is taken from odd position tile bytes */

        uint8_t  pxa[8];

        for (j=0; j<8; j++)
        {
             uint8_t shft = (1 << j);

             pxa[j] = ((*(tiles + tile_ptr) & shft) ? 1 : 0) |
                      ((*(tiles + tile_ptr + 1) & shft) ? 2 : 0);
        }

        /* set 8 pixels (full tile line) */
        for (i=0; i<8; i++)
        {
            if (oam->x_flip)
                off = i;
            else
                off = 7 - i; 

            /* is it on screen? */
            fb_x = x + off;

            if (fb_x < 0 || fb_x > 160)
                continue;

            /* set serial position on frame buffer */
            pos = pos_fb + off;

            /* is it inside the screen? */
            if (pos >= 144 * 160 || pos < 0)
                continue;

            if (global_cgb)
            {
                if (pxa[i] != 0x00) 
                {
                    gpu.frame_buffer[pos] = palette[pxa[i]];
                    gpu.priority[pos] = !oam->priority;
                }
            }
            else
            {
                /* push on screen pixels not set to zero (transparent) */
                /* and if the priority is set to one, overwrite just   */
                /* bg pixels set to zero                               */
                if ((pxa[i] != 0x00) &&
                    (oam->priority == 0 || 
                    (oam->priority == 1 && 
                     gpu.frame_buffer[pos] == gpu.bg_palette[0x00])))
                {
                    gpu.frame_buffer[pos] = palette[pxa[i]];
                    gpu.priority[pos] = !oam->priority;
                }
            }
        }
    }
}

/* update GPU internal state given CPU T-states */
void gpu_step()
{
    char ly_changed = 0;
    char mode_changed = 0;

    /* advance only in case of turned on display */
    if ((*gpu.lcd_ctrl).display == 0)
        return;

    /* update clock counter */
    if (global_double_speed)
        gpu.clocks += 2;
    else
        gpu.clocks += 4;

    /* take different action based on current state */
    switch((*gpu.lcd_status).mode)
    {
        /*
         * during HBLANK (CPU can access VRAM)
         */
        case 0: /* check if an HBLANK is complete (201 t-states) */
                if (gpu.clocks >= 204)
                {
                    /*
                     * if current line == 143 (and it's about to turn 144)
                     * enter mode 01 (VBLANK)
                     */
                    if (*gpu.ly == 143)
                    {
                        /* notify mode has changes */
                        mode_changed = 1;

                        (*gpu.lcd_status).mode = 0x01;

                        /* DRAW! TODO */
                        /* CHECK INTERRUPTS! TODO */

                        /* set VBLANK interrupt flag */
                        gpu_if->lcd_vblank = 1;

                        /* update frame in memory */
                        // gpu_update_frame_buffer();

                        /* and finally push it on screen! */
                        gpu_draw_frame();
                    } 
                    else
                    {
                        /* notify mode has changed */
                        mode_changed = 1;

                        /* enter OAM mode */
                        (*gpu.lcd_status).mode = 0x02;
                    }

                    /* notify mode has changed */
                    ly_changed = 1;

                    /* inc current line */
                    (*gpu.ly)++;

                    /* reset clock counter */
                    gpu.clocks -= 204;
                }

                break;

        /*
         * during VBLANK (CPU can access VRAM)
         */
        case 1: /* check if an HBLANK is complete (456 t-states) */
                if (gpu.clocks >= 456) 
                {
                    /* reset clock counter */
                    gpu.clocks -= 456;

                    /* notify ly has changed */
                    ly_changed = 1;

                    /* inc current line */
                    (*gpu.ly)++;

                    /* reached the bottom? */
                    if ((*gpu.ly) > 153)
                    {
                        /* go back to line 0 */
                        (*gpu.ly) = 0;

                        /* switch to OAM mode */
                        (*gpu.lcd_status).mode = 0x02;
                    }
                }

                break;

        /*
         * during OAM (LCD access FE00-FE90, so CPU cannot)
         */
        case 2: /* check if an OAM is complete (80 t-states) */
                if (gpu.clocks >= 80)
                {
                    /* reset clock counter */
                    gpu.clocks -= 80;

                    /* notify mode has changed */
                    mode_changed = 1;

                    /* switch to VRAM mode */
                    (*gpu.lcd_status).mode = 0x03;
                }

                break;

        /*
         * during VRAM (LCD access both OAM and VRAM, so CPU cannot)
         */
        case 3: /* check if a VRAM read is complete (172 t-states) */
                if (gpu.clocks >= 172)
                {
                    /* reset clock counter */
                    gpu.clocks -= 172;

                    /* notify mode has changed */
                    mode_changed = 1;

                    /* go back to HBLANK mode */
                    (*gpu.lcd_status).mode = 0x00;

                    /* draw line */
                    gpu_draw_line(*gpu.ly);
                }

                break;

    }

    /* ly changed? is it the case to trig an interrupt? */
    if (ly_changed)
    {
        /* check if we gotta trigger an interrupt */
        if ((*gpu.ly) == (*gpu.lyc))
        { 
            /* set lcd status flags indicating there's a concidence */
            (*gpu.lcd_status).ly_coincidence = 1;

            /* an interrupt is desiderable? */
            if ((*gpu.lcd_status).ir_ly_coincidence)
                gpu_if->lcd_ctrl = 1;
        }
        else
        {
            /* set lcd status flags indicating there's NOT a concidence */
            (*gpu.lcd_status).ly_coincidence = 0;
        }
    }

    /* mode changed? is is the case to trig an interrupt? */
    if (mode_changed)
    {
        if ((*gpu.lcd_status).mode == 0x00 &&
            (*gpu.lcd_status).ir_mode_00)
            gpu_if->lcd_ctrl = 1;
        else if ((*gpu.lcd_status).mode == 0x01 &&
                 (*gpu.lcd_status).ir_mode_01)
            gpu_if->lcd_ctrl = 1;
        else if ((*gpu.lcd_status).mode == 0x02 &&
                 (*gpu.lcd_status).ir_mode_10)
            gpu_if->lcd_ctrl = 1;
    }
}

uint8_t gpu_read_reg(uint16_t a)
{
    switch (a)
    {
        case 0xFF69:

            if ((gpu.cgb_palette_bg_idx & 0x01) == 0x00)
                return gpu.cgb_palette_bg[gpu.cgb_palette_bg_idx / 2] & 0x00ff;
            else
                return (gpu.cgb_palette_bg[gpu.cgb_palette_bg_idx / 2] & 0xff00) >> 8;

    }

    return 0x00;
}

void gpu_write_reg(uint16_t a, uint8_t v)
{
    int i;
    uint8_t r,g,b;

    switch (a)
    {
        case 0xFF47:

            gpu.bg_palette[0] = gpu_color_lookup[v & 0x03]; 
            gpu.bg_palette[1] = gpu_color_lookup[(v & 0x0c) >> 2];
            gpu.bg_palette[2] = gpu_color_lookup[(v & 0x30) >> 4];
            gpu.bg_palette[3] = gpu_color_lookup[(v & 0xc0) >> 6];

            break;

        case 0xFF48:

            gpu.obj_palette_0[0] = gpu_color_lookup[v & 0x03]; 
            gpu.obj_palette_0[1] = gpu_color_lookup[(v & 0x0c) >> 2];
            gpu.obj_palette_0[2] = gpu_color_lookup[(v & 0x30) >> 4];
            gpu.obj_palette_0[3] = gpu_color_lookup[(v & 0xc0) >> 6];

            break;

        case 0xFF49:

            gpu.obj_palette_1[0] = gpu_color_lookup[v & 0x03];
            gpu.obj_palette_1[1] = gpu_color_lookup[(v & 0x0c) >> 2];
            gpu.obj_palette_1[2] = gpu_color_lookup[(v & 0x30) >> 4];
            gpu.obj_palette_1[3] = gpu_color_lookup[(v & 0xc0) >> 6];

            break;

        case 0xFF68:

            gpu.cgb_palette_bg_idx = v & 0x3f;
            gpu.cgb_palette_bg_autoinc = ((v & 0x80) == 0x80);

            break;

        case 0xFF69:

            i = gpu.cgb_palette_bg_idx / 2;

            if ((gpu.cgb_palette_bg_idx & 0x01) == 0x00)
            {
                gpu.cgb_palette_bg[i] &= 0xff00;
                gpu.cgb_palette_bg[i] |= v;
            }
            else
            {
                gpu.cgb_palette_bg[i] &= 0x00ff;
                gpu.cgb_palette_bg[i] |= (v << 8); 
            }

            r = gpu.cgb_palette_bg[i] & 0x1F;
            g = gpu.cgb_palette_bg[i] >> 5 & 0x1F;
            b = gpu.cgb_palette_bg[i] >> 10 & 0x1F;

   	    gpu.cgb_palette_bg_rgb565[i] = 
                (((r * 13 + g * 2 + b + 8) << 7) & 0xF800) |
                 ((g * 3 + b + 1) >> 1) << 5 |
                 ((r * 3 + g * 2 + b * 11 + 8) >> 4);
 
            if (gpu.cgb_palette_bg_autoinc)
                gpu.cgb_palette_bg_idx++;

            break;

        case 0xFF6A:

            gpu.cgb_palette_oam_idx = v & 0x3f;
            gpu.cgb_palette_oam_autoinc = ((v & 0x80) == 0x80);

            break;

        case 0xFF6B:

            i = gpu.cgb_palette_oam_idx / 2;

            if ((gpu.cgb_palette_oam_idx & 0x01) == 0x00)
            {
                gpu.cgb_palette_oam[i] &= 0xff00;
                gpu.cgb_palette_oam[i] |= v;
            }
            else
            {
                gpu.cgb_palette_oam[i] &= 0x00ff;
                gpu.cgb_palette_oam[i] |= (v << 8);
            }

            r = gpu.cgb_palette_oam[i] & 0x1F;
            g = gpu.cgb_palette_oam[i] >> 5 & 0x1F;
            b = gpu.cgb_palette_oam[i] >> 10 & 0x1F;

            gpu.cgb_palette_oam_rgb565[i] =
                (((r * 13 + g * 2 + b + 8) << 7) & 0xF800) |
                 ((g * 3 + b + 1) >> 1) << 5 |
                 ((r * 3 + g * 2 + b * 11 + 8) >> 4);

            if (gpu.cgb_palette_oam_autoinc)
                gpu.cgb_palette_oam_idx++;

            break;

    }
}