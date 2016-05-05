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

#ifndef __MMU__
#define __MMU__

#include "subsystem/gameboy/cycles_hdr.h"
#include "subsystem/gameboy/gpu_hdr.h"
#include "subsystem/gameboy/interrupts_hdr.h"
#include "subsystem/gameboy/input_hdr.h"
#include "subsystem/gameboy/mmu_hdr.h"
#include <stdlib.h>

/* GAMEBOY MEMORY AREAS 

0x0000 - 0x00FF - BIOS
0x0000 - 0x3FFF - First 16k of game ROM (permanent)
0x4000 - 0x7FFF - ROM banks (switchable)
0x8000 - 0x9FFF - Video RAM (8kb - keeps pixels data) 
0xA000 - 0xBFFF - External RAM (switchable, it was on cartridge,
                                8kb banks, max 32k, NON volatile)
0xC000 - 0xDFFF - Gameboy RAM
0xE000 - 0xEFFF - ????????????????
0xFE00 - 0xFF7F - I/O
0xFF80 - 0xFFFE - Temp RAM
0xFFFF          - Turn on/off interrupts

*/

/* cartridge type */
uint8_t carttype;

/* number of switchable roms */
uint8_t roms;

/* system memory (!= cartridge memory) */
uint8_t memory[65536];

/* cartridge memory (max 2MB) */
uint8_t cart_memory[2 << 21];

/* RAM memory area */
uint8_t *ram;

/* current bank in case of MBC controller */
uint8_t rom_current_bank = 1;
uint8_t ram_current_bank = 1;

/* banking mode - 0 ROM, 1 RAM */
uint8_t banking = 0;

/* init (alloc) system state.memory */
void static mmu_init(uint8_t c, uint8_t rn)
{
    rom_current_bank = 0x01;
    ram_current_bank = 0x01;

    carttype = c;
    roms = rn;
}

/* init (alloc) system state.memory */
void static mmu_init_ram(uint32_t c)
{
    ram = malloc(c);

    bzero(ram, c);
}

/* load data in a certain address */
void static mmu_load(uint8_t *data, size_t sz, uint16_t a)
{
    memcpy(&memory[a], data, sz);
}

/* load full cartridge */
void static mmu_load_cartridge(uint8_t *data, size_t sz)
{
    /* copy max 32k into working memory */
    memcpy(memory, data, 2 << 14);

    /* copy full cartridge */
    memcpy(cart_memory, data, sz);
}

/* move 8 bit from s to d */
void static __always_inline mmu_move(uint16_t d, uint16_t s)
{
//    cycles_step(8);

    mmu_write(d, mmu_read(s));

//    memory[d] = memory[s];
}

/* return absolute memory address */
void static __always_inline *mmu_addr(uint16_t a)
{
//    cycles_step();

    return (void *) &memory[a];
}

/* read 8 bit data from a memory addres (not affecting cycles) */
uint8_t static __always_inline mmu_read_no_cyc(uint16_t a)
{
    if (a >= 0xE000 && a <= 0xFDFF)
        return memory[a - 0x2000];

    return memory[a];
}

/* read 8 bit data from a memory addres */
uint8_t static __always_inline mmu_read(uint16_t a)
{
    /* always takes 4 cycles */
    cycles_step(4);

    /* joypad reading */
    if (a == 0xFF00)
        return input_get_keys(memory[a]);

    /* RAM mirror */
    if (a >= 0xE000 && a <= 0xFDFF)
        return memory[a - 0x2000];

    return memory[a];
}

/* write 16 bit block on a memory address (no cycles affected) */
void static __always_inline mmu_write_no_cyc(uint16_t a, uint8_t v)
{
    memory[a] = v;
}

/* write 16 bit block on a memory address */
void static __always_inline mmu_write(uint16_t a, uint8_t v)
{
    /* update cycles AFTER memory set */
    cycles_step(4);

    /* wanna write on ROM? */
    if (a < 0x8000)
    {
        /* return in case of ONLY ROM */
        if (carttype == 0x00)
            return;

        /* TODO - MBC strategies */

        switch (carttype)
        {
            /* MBC1 */
            case 0x01: 
            case 0x02:  
            case 0x03: 

                       if (a >= 0x2000 && a <= 0x3FFF) 
                       {
                           /* reset 5 bits */
                           uint8_t b = rom_current_bank & 0xE0;

                           /* set them with new value */
                           b |= v & 0x1F;

                           /* filter with max ROM available */
                           // uint8_t filter = 0xFF >> (8 - roms);

                           /* filter result to get a value < max rom number */
                           // b &= filter;
 
                           if (b != rom_current_bank)
                           {
                                /* copy! */
                                memcpy(&memory[0x4000], &cart_memory[b * 0x4000], 0x4000);
                           }
                          
                           rom_current_bank = b;
                       }
                       else if (a >= 0x4000 && a <= 0x5FFF)
                       {
                           /* ROM banking? it's about 2 higher bits */
                           if (banking == 0)
                           {
                               /* reset 5 bits */
                               uint8_t b = rom_current_bank & 0x1F;

                               /* set them with new value */
                               b |= (v << 5);

                               /* filter with max ROM available */
                               uint8_t filter = 0xFF >> (8 - roms);

                               /* filter result to get a value < max rom number */
                               b &= filter;

                               if (b != rom_current_bank)
                               {
                                   /* copy! */
                                   memcpy(&memory[0x4000], &cart_memory[b * 0x4000], 0x4000);
                               }

                               rom_current_bank = b;
                           }
                           else
                           {
                               /* save current bank */
                               memcpy(&ram[0x2000 * ram_current_bank], 
                                      &memory[0xA000], 0x2000);

                               ram_current_bank = v;

                               /* move new ram bank */
                               memcpy(&memory[0xA000], &ram[0x2000 * ram_current_bank], 
                                      0x2000);
                           }
                       }
                       else if (a >= 0x6000 && a <= 0x7FFF)
                       {
                           banking = v;
                       }

        }

        return; 
    }

    if (a >= 0xE000 && a <= 0xFDFF)
    {
        memory[a - 0x2000] = v;
        return;
    } 

    /* accessing VRAM */
    if (a >= 0x8000 && a <= 0x9FFF)
    {
        /* can't access vram! */
        if ((*gpu_state.lcd_status).mode == 0x03)
            return; 

    }

    /* accessing OAM */
    if (a >= 0xFE00 && a <= 0xFE9F)
    {
        /* can't access OAM or VRAM! */
        if ((*gpu_state.lcd_status).mode == 0x02 || 
            (*gpu_state.lcd_status).mode == 0x03)
            return;
    }

    /* resetting timer DIV */
    if (a == 0xFF04)
    {
        memory[a] = 0x00;
        return;
    }

    /* LCD turned on/off? */
    if (a == 0xFF40)
    {
        if ((v ^ memory[0xFF40]) & 0x01)
            gpu_toggle(v);
    }

    /* finally set memory byte with data */
    memory[a] = v;

    if (a == 0xFF46)
    {
        uint16_t src = v * 256;

        if ((*gpu_state.lcd_status).mode == 0x02 || 
            (*gpu_state.lcd_status).mode == 0x03)
            return;

        /* copy an entire block of memory into OAM area */
        memcpy(&memory[0xFE00], &memory[src], 160);
    } 

    if (a == 0xFF47)
    {
        gpu_state.bg_palette[0] = gpu_color_lookup[v & 0x03];
        gpu_state.bg_palette[1] = gpu_color_lookup[(v & 0x0c) >> 2];
        gpu_state.bg_palette[2] = gpu_color_lookup[(v & 0x30) >> 4];
        gpu_state.bg_palette[3] = gpu_color_lookup[(v & 0xc0) >> 6];
    }

    if (a == 0xFF48)
    {
        gpu_state.obj_palette_0[0] = gpu_color_lookup[v & 0x03];
        gpu_state.obj_palette_0[1] = gpu_color_lookup[(v & 0x0c) >> 2];
        gpu_state.obj_palette_0[2] = gpu_color_lookup[(v & 0x30) >> 4];
        gpu_state.obj_palette_0[3] = gpu_color_lookup[(v & 0xc0) >> 6];
    }

    if (a == 0xFF49)
    {
        gpu_state.obj_palette_1[0] = gpu_color_lookup[v & 0x03];
        gpu_state.obj_palette_1[1] = gpu_color_lookup[(v & 0x0c) >> 2];
        gpu_state.obj_palette_1[2] = gpu_color_lookup[(v & 0x30) >> 4];
        gpu_state.obj_palette_1[3] = gpu_color_lookup[(v & 0xc0) >> 6];
    }

    if (0 && a == 0xFF40)
    {
/*            printf("BG: %d - Sprites: %d - Sp Size: %d - BG Tile Map: %d\n",
                   (*gpu_state.lcd_ctrl).bg, (*gpu_state.lcd_ctrl).sprites,
                   (*gpu_state.lcd_ctrl).sprites_size,
                   (*gpu_state.lcd_ctrl).bg_tiles_map);

            printf("BG TILES: %d - Window: %d - Window tile: %d - Display: %d\n",
                   (*gpu_state.lcd_ctrl).bg_tiles, (*gpu_state.lcd_ctrl).window,
                   (*gpu_state.lcd_ctrl).window_tiles_map, (*gpu_state.lcd_ctrl).display); */
    }
}

/* read 16 bit data from a memory addres */
unsigned int static __always_inline mmu_read_16(uint16_t a)
{
    /* 16 bit read = +8 cycles */
    cycles_step(8);

    return (memory[a] | (memory[a + 1] << 8));
}

/* write 16 bit block on a memory address */
void static __always_inline mmu_write_16(uint16_t a, uint16_t v)
{
    memory[a] = (uint8_t) (v & 0x00ff);
    memory[a + 1] = (uint8_t) (v >> 8);

    /* 16 bit write = +8 cycles */
    cycles_step(8);
}

#endif
