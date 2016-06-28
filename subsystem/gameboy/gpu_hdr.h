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

#ifndef __GPU_HDR__
#define __GPU_HDR__

/* prototypes */
void static                 gpu_init();
void static __always_inline gpu_step(uint8_t t);
void static                 gpu_toggle(uint8_t state);

/* Gameboy LCD Control - R/W accessing 0xFF40 address */
typedef struct gpu_lcd_ctrl_s
{
    uint8_t bg:1;                   /* 0 = BG off, 1 = BG on        */ 
    uint8_t sprites:1;              /* ???                          */
    uint8_t sprites_size:1;         /* 0 = 8x8, 1 = 8x16            */
    uint8_t bg_tiles_map:1;         /* 0 = 9800-9BFF, 1 = 9C00-9FFF */
    uint8_t bg_tiles:1;             /* 0 = 8800-97FF, 1 = 8000-8FFF */
    uint8_t window:1;               /* 0 = window off, 1 = on       */
    uint8_t window_tiles_map:1;     /* 0 = 9800-9BFF, 1 = 9C00-9FFF */
    uint8_t display:1;              /* 0 = LCD off, 1 = LCD on      */
} gpu_lcd_ctrl_t; 

/* Gameboy LCD Status - R/W accessing 0xFF41 address */
typedef struct gpu_lcd_status_s
{
    uint8_t mode:2;
    uint8_t ly_coincidence:1;
    uint8_t ir_mode_00:1;
    uint8_t ir_mode_01:1;
    uint8_t ir_mode_10:1;
    uint8_t ir_ly_coincidence:1;
    uint8_t spare:1;
} gpu_lcd_status_t;

/* Gameboy GPU status */
typedef struct gpu_s 
{
    gpu_lcd_ctrl_t   *lcd_ctrl;
    gpu_lcd_status_t *lcd_status;

    /* scroll positions */
    uint8_t   *scroll_x;
    uint8_t   *scroll_y;

    /* window position  */
    uint8_t   *window_x;
    uint8_t   *window_y;

    /* current scanline and it's compare values */
    uint8_t   *ly;
    uint8_t   *lyc;

    /* clocks counter   */
    uint32_t  clocks;

    /* BG palette       */
    uint32_t  bg_palette[4]; 

    /* Obj palette 0/1  */
    uint32_t  obj_palette_0[4]; 
    uint32_t  obj_palette_1[4]; 

    /* frame buffer     */
    uint32_t  frame_buffer[160 * 144];
    char      priority[160 * 144];

} gpu_t;

/* global state of GPU */
gpu_t gpu;

/* 2 bit to 8 bit color lookup */
static uint32_t gpu_color_lookup[] = { 0x00FFFFFF, 0x00AAAAAA, 0x00555555, 0x00000000 };

/* TEST */
uint32_t gpu_total_cycles = 0;

/* exported functions */
void gpu_write_reg(uint16_t a, uint8_t v);

#endif
