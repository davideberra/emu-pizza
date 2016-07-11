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
uint8_t cart_memory[1 << 20];

/* RAM memory area */
uint8_t *ram;
uint32_t ram_sz;

/* current bank in case of MBC controller */
uint8_t rom_current_bank = 1;
uint8_t ram_current_bank = 0;

/* banking mode - 0 ROM, 1 RAM */
uint8_t banking = 0;

/* DMA counter and address */
uint16_t mmu_dma_address = 0;
uint16_t mmu_dma_cycles = 0;

/* init (alloc) system state.memory */
void static mmu_init(uint8_t c, uint8_t rn)
{
    rom_current_bank = 0x01;
    ram_current_bank = 0x00;

    /* save carttype and qty of ROM blocks */
    carttype = c;
    roms = rn;
}

/* init (alloc) system state.memory */
void static mmu_init_ram(uint32_t c)
{
    ram_sz = c;

    ram = malloc(c);

    bzero(ram, c);
}

/* update DMA internal state given CPU T-states */
void static __always_inline mmu_step(uint8_t t)
{
    if (mmu_dma_address != 0x00)
    {
        mmu_dma_cycles -= 4;
    
        /* enough cycles passed? */
        if (mmu_dma_cycles == 0)
        {
            memcpy(&memory[0xFE00], &memory[mmu_dma_address], 160);

            /* reset address */
            mmu_dma_address = 0x00;
        }
    }
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
    mmu_write(d, mmu_read(s));
}

/* return absolute memory address */
void static __always_inline *mmu_addr(uint16_t a)
{
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
                           uint8_t filter = 0xFF >> (7 - roms);

                           /* filter result to get a value < max rom number */
                           b &= filter;

                           /* 0x00 is not valid, switch it to 0x01 */
                           if (b == 0x00)
                               b = 0x01;
                 
                           if (b != rom_current_bank)
                           {
                                /* copy new rom bank! */
                                memcpy(&memory[0x4000], 
                                       &cart_memory[b * 0x4000], 0x4000);
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
                               uint8_t filter = 0xFF >> (7 - roms);

                               /* filter result to get a value < max rom number */
                               b &= filter;

                               if (b != rom_current_bank)
                               {
                                   /* copy! */
                                   memcpy(&memory[0x4000], 
                                          &cart_memory[b * 0x4000], 0x4000);
                               }

                               rom_current_bank = b;
                           }
                           else
                           {
                               if ((0x2000 * v) < ram_sz)
                               { 
                                   /* save current bank */
                                   memcpy(&ram[0x2000 * ram_current_bank], 
                                          &memory[0xA000], 0x2000);
  
                                   ram_current_bank = v;

                                   /* move new ram bank */
                                   memcpy(&memory[0xA000], 
                                          &ram[0x2000 * ram_current_bank], 
                                          0x2000);
                               }
                           }
                       }
                       else if (a >= 0x6000 && a <= 0x7FFF)
                       {
                           banking = v;
                       }

                       break;

            /* MBC2 */
            case 0x05:
            case 0x06: 

                       if (a >= 0x2000 && a <= 0x3FFF) 
                       {
                           /* use lower nibble to set current bank */
                           uint8_t b = v & 0x0f;

                           if (b != rom_current_bank)
                               memcpy(&memory[0x4000], 
                                      &cart_memory[b * 0x4000], 0x4000);

                           rom_current_bank = b;
                       }
        }

        return; 
    }

    if (a >= 0xE000)
    {

    /* changes on sound registers? */
    if (a >= 0xFF10 && a <= 0xFF26)
    {
        /* set memory */
        memory[a] = v;

        /* then generate samples! */
        sound_write_reg(a, v);

        return;
    }

    /* mirror area */
    if (a >= 0xE000 && a <= 0xFDFF)
    {
        memory[a - 0x2000] = v;
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
        if ((v ^ memory[0xFF40]) & 0x80)
            gpu_toggle(v);
    }

    /* palette update */
    if (a >= 0xFF47 && a <= 0xFF49)
        gpu_write_reg(a, v);

    /* finally set memory byte with data */
    memory[a] = v;

    /* DMA access */
    if (a == 0xFF46)
    {
        /* calc source address */ 
        mmu_dma_address = v * 256;

        /* initialize counter, DMA needs 672 ticks */
        mmu_dma_cycles = 168;
    }
    }
    else
        memory[a] = v; 
}

/* read 16 bit data from a memory addres */
unsigned int static __always_inline mmu_read_16(uint16_t a)
{
    /* 16 bit read = +8 cycles */
    cycles_step(4);
    cycles_step(4);

    return (memory[a] | (memory[a + 1] << 8));
}

/* write 16 bit block on a memory address */
void static __always_inline mmu_write_16(uint16_t a, uint16_t v)
{
    memory[a] = (uint8_t) (v & 0x00ff);
    memory[a + 1] = (uint8_t) (v >> 8);

    /* 16 bit write = +8 cycles */
    cycles_step(4);
    cycles_step(4);
}

void mmu_save_ram(char *fn)
{
    /* save only if cartridge got a battery */
    if (carttype == 0x03 || carttype == 0x06)
    {
        FILE *fp = fopen(fn, "w+");

        if (fp == NULL)
        {
            printf("Error dumping RAM\n");
            return;
        } 

        if (ram_sz <= 0x2000)
        {
            /* no need to put togheter pieces of ram banks */
            fwrite(&memory[0xA000], ram_sz, 1, fp);
        }
    }
}

void mmu_restore_ram(char *fn)
{   
    /* save only if cartridge got a battery */
    if (carttype == 0x03 || carttype == 0x06)
    {
        FILE *fp = fopen(fn, "r+");

        /* it could be not present */
        if (fp == NULL)
            return;

        if (ram_sz <= 0x2000)
        {
            /* no need to put togheter pieces of ram banks */
            fread(&memory[0xA000], ram_sz, 1, fp);
        }
    } 
}

#endif
