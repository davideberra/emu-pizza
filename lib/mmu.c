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

#include "cycles.h"
#include "global.h"
#include "gpu.h"
#include "interrupt.h"
#include "input.h"
#include "mmu.h"
#include "sound.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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

/* cartridge memory (max 8MB) */
uint8_t cart_memory[1 << 22];

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


typedef struct mmu_s {

    /* vram in standby */
    uint8_t vram0[0x2000];
    uint8_t vram1[0x2000];

    /* vram current idx */
    uint8_t vram_idx;

    /* internal RAM */
    uint8_t ram_internal[0x2000];
    uint8_t ram_external_enabled;
    uint8_t ram_current_bank;

    /* working RAM (only CGB) */
    uint8_t wram[0x8000];

    /* current WRAM bank (only CGB) */
    uint8_t wram_current_bank;

    /* HDMA transfer stuff */
    uint16_t hdma_src_address;
    uint16_t hdma_dst_address;
    uint16_t hdma_to_transfer;
    uint8_t  hdma_transfer_mode;
    uint8_t  hdma_current_line;

} mmu_t;

mmu_t mmu;


/* init (alloc) system state.memory */
void mmu_init(uint8_t c, uint8_t rn)
{
    rom_current_bank = 0x01;
    ram_current_bank = 0x00;

    /* set ram to NULL */
    ram = NULL;

    /* save carttype and qty of ROM blocks */
    carttype = c;
    roms = rn;

    mmu.vram_idx = 0;
    mmu.wram_current_bank = 1;
    mmu.ram_current_bank = 0;
    mmu.ram_external_enabled = 0;
}

/* init (alloc) system state.memory */
void mmu_init_ram(uint32_t c)
{
    ram_sz = c;

    ram = malloc(c);

    bzero(ram, c);
}

/* update DMA internal state given CPU T-states */
void mmu_step()
{
    /* DMA */
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

    /* HDMA (only CGB) */
    if (global_cgb && mmu.hdma_to_transfer)
    {
        /* hblank transfer */
        if (mmu.hdma_transfer_mode)
        {
            if (memory[0xFF44] < 143 && 
                mmu.hdma_current_line != memory[0xFF44])
            {
                /* update current line */
                mmu.hdma_current_line = memory[0xFF44];

                /* move! */
                memcpy(&memory[mmu.hdma_dst_address + 
                               mmu.hdma_to_transfer - 0x10],
                       &memory[mmu.hdma_src_address +
                               mmu.hdma_to_transfer - 0x10], 0x10);

                /* decrease bytes to transfer */
                mmu.hdma_to_transfer -= 0x10;
            }
        }
    }
}

/* load data in a certain address */
void mmu_load(uint8_t *data, size_t sz, uint16_t a)
{
    memcpy(&memory[a], data, sz);
}

/* load full cartridge */
void mmu_load_cartridge(uint8_t *data, size_t sz)
{
    /* copy max 32k into working memory */
    memcpy(memory, data, 2 << 14);

    /* copy full cartridge */
    memcpy(cart_memory, data, sz);
}

/* move 8 bit from s to d */
void mmu_move(uint16_t d, uint16_t s)
{
    mmu_write(d, mmu_read(s));
}

/* return absolute memory address */
void *mmu_addr(uint16_t a)
{
    return (void *) &memory[a];
}

/* return absolute memory address */
void *mmu_addr_vram0()
{
    return (void *) &mmu.vram0;
}

/* return absolute memory address */
void *mmu_addr_vram1()
{
    return (void *) &mmu.vram1;
}

/* read 8 bit data from a memory addres (not affecting cycles) */
uint8_t mmu_read_no_cyc(uint16_t a)
{
    if (a >= 0xE000 && a <= 0xFDFF)
        return memory[a - 0x2000];

    return memory[a];
}

/* read 8 bit data from a memory addres */
uint8_t mmu_read(uint16_t a)
{
    /* always takes 4 cycles */
    cycles_step();

    /* joypad reading */
    if (a == 0xFF00)
        return input_get_keys(memory[a]);

    /* RAM mirror */
    if (a >= 0xE000 && a <= 0xFDFF)
        return memory[a - 0x2000];

    /* changes on sound registers? */
    if (a >= 0xFF10 && a <= 0xFF3F)
        return sound_read_reg(a, memory[a]);

    /* only CBG registers */
    if (global_cgb)
    {
        switch (a)
        {
            case 0xFF55:
                /* HDMA result */
                if (mmu.hdma_to_transfer)
                    return 0x00;
                else
                    return 0xFF;

            case 0xFF69:
                return gpu_read_reg(a);

        }
  
        /* VRAM write? */
        if (a >= 0x8000 && a < 0xA000)
        {
            if (mmu.vram_idx == 0)
                return mmu.vram0[a - 0x8000];
            else
                return mmu.vram1[a - 0x8000];
        }
    }

    return memory[a];
}

/* write 16 bit block on a memory address (no cycles affected) */
void mmu_write_no_cyc(uint16_t a, uint8_t v)
{
    memory[a] = v;
}

/* write 16 bit block on a memory address */
void mmu_write(uint16_t a, uint8_t v)
{
    /* update cycles AFTER memory set */
    cycles_step();

    /* color gameboy stuff */
    if (global_cgb)
    {
        /* VRAM write? */
        if (a >= 0x8000 && a < 0xA000)
        {
            if (mmu.vram_idx == 0)
                mmu.vram0[a - 0x8000] = v;
            else
                mmu.vram1[a - 0x8000] = v;

//            memory[a] = v;

            return;
        }
            
        /* switch WRAM */
        if (a == 0xFF70)
        {
            /* number goes from 1 to 7 */
            uint8_t new = (v & 0x07);

            if (new == 0) 
                new = 1;

            if (new == mmu.wram_current_bank)
                return;

            /* save current bank */
            memcpy(&mmu.wram[0x1000 * mmu.wram_current_bank],
                   &memory[0xD000], 0x1000);

            mmu.wram_current_bank = new;

            /* move new ram bank */
            memcpy(&memory[0xD000],
                   &mmu.wram[0x1000 * mmu.wram_current_bank],
                   0x1000);

            return;
        }

        if (a == 0xFF4F)
        {
            mmu.vram_idx = (v & 0x01);

            return;
        }
    }

    /* wanna write on ROM? */
    if (a < 0x8000)
    {
        /* return in case of ONLY ROM */
        if (carttype == 0x00)
            return;

        /* TODO - MBC strategies */
        uint8_t b = rom_current_bank;

        switch (carttype)
        {
            /* MBC1 */
            case 0x01: 
            case 0x02:  
            case 0x03: 

                if (a >= 0x2000 && a <= 0x3FFF) 
                {
                    /* reset 5 bits */
                    b = rom_current_bank & 0xE0;

                    /* set them with new value */
                    b |= v & 0x1F;

                    /* doesn't fit on max rom number? */
                    if (b > (2 << roms))
                    {
                        /* filter result to get a value < max rom number */
                        b %= (2 << roms);
                    }

                    /* 0x00 is not valid, switch it to 0x01 */
                    if (b == 0x00)
                        b = 0x01;
                }
                else if (a >= 0x4000 && a <= 0x5FFF)
                {
                    /* ROM banking? it's about 2 higher bits */
                    if (banking == 0)
                    {
                        /* reset 5 bits */
                        b = rom_current_bank & 0x1F;

                        /* set them with new value */
                        b |= (v << 5);

                        /* doesn't fit on max rom number? */
                        if (b > (2 << roms))
                        {
                            /* filter result to get a value < max rom number */
                            b %= (2 << roms);
                        }
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
                    banking = v;

                break;

            /* MBC2 */
            case 0x05:
            case 0x06: 

                if (a >= 0x2000 && a <= 0x3FFF) 
                {
                    /* use lower nibble to set current bank */
                    b = v & 0x0f;

                    /*if (b != rom_current_bank)
                        memcpy(&memory[0x4000], 
                               &cart_memory[b * 0x4000], 0x4000);

                    rom_current_bank = b;*/
                }

                break;

            /* MBC3 */
            case 0x10:
            case 0x13:

                if (a >= 0x2000 && a <= 0x3FFF)
                {
                    /* set them with new value */
                    b = v & 0x7F;

                    /* doesn't fit on max rom number? */
                    if (b > (2 << roms))
                    {
                        /* filter result to get a value < max rom number */
                        b %= (2 << roms);
                    }

                    /* 0x00 is not valid, switch it to 0x01 */
                    if (b == 0x00)
                        b = 0x01;
                }

                break;

            /* MBC5 */
            case 0x19:
            case 0x1B:
            case 0x1C:

                if (a >= 0x0000 && a <= 0x1FFF)
                {
                    if (v == 0x0A)
                    {
                        /* already enabled? */
                        if (mmu.ram_external_enabled)
                            return;

                        /* save current bank */
                        memcpy(mmu.ram_internal,
                               &memory[0xA000], 0x2000);

                        /* restore external ram bank */
                        memcpy(&memory[0xA000],
                               &ram[0x2000 * ram_current_bank],
                               0x2000);

                        return;
                    }

                    if (v == 0x00)
                    {
                        /* already disabled? */
                        if (mmu.ram_external_enabled == 0)
                            return;

                        /* save current bank */
                        memcpy(&ram[0x2000 * ram_current_bank], 
                               &memory[0xA000], 0x2000);

                        /* restore external ram bank */
                        memcpy(&memory[0xA000],
                               mmu.ram_internal, 0x2000);


                    }

                }
                if (a >= 0x2000 && a <= 0x2FFF)
                {
                    /* set them with new value */
                    b = (rom_current_bank & 0xFF00) | v;

                    /* doesn't fit on max rom number? */
                    if (b > (2 << roms))
                    {
                        /* filter result to get a value < max rom number */
                        b %= (2 << roms);
                    }
                }
                else if (a >= 0x3000 && a <= 0x3FFF)
                {
                    /* set them with new value */
                    b = (rom_current_bank & 0x00FF) | ((v & 0x01) << 8);

                    /* doesn't fit on max rom number? */
                    if (b > (2 << roms))
                    {
                        /* filter result to get a value < max rom number */
                        b %= (2 << roms);
                    }
                }
                else if (a >= 0x4000 && a <= 0x5FFF)
                {
                    if ((0x2000 * (v & 0x0f)) < ram_sz)
                    {
                        /* save current bank */
                        memcpy(&ram[0x2000 * ram_current_bank],
                               &memory[0xA000], 0x2000);

                        ram_current_bank = v & 0x0f;

                        /* move new ram bank */
                        memcpy(&memory[0xA000],
                               &ram[0x2000 * ram_current_bank],
                               0x2000);
                    }
                }

                break;

        }

        /* need to switch? */
        if (b != rom_current_bank)
        {
             /* copy from cartridge rom to GB switchable bank area */
             memcpy(&memory[0x4000], &cart_memory[b * 0x4000], 0x4000);

             /* save new current bank */
             rom_current_bank = b;
        }

        return; 
    }

    if (a >= 0xE000)
    {
        /* changes on sound registers? */
        if (a >= 0xFF10 && a <= 0xFF3F)
        {
            /* set memory */
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

        /* only 4 high bits are writable */
        if (a == 0xFF41)
        {
            memory[a] = (memory[a] & 0x0f) | (v & 0xf0);
            return;
        }
            
        /* palette update */
        if ((a >= 0xFF47 && a <= 0xFF49) ||
            (a >= 0xFF68 && a <= 0xFF6B))
            gpu_write_reg(a, v);

        /* CGB only registers */
        if (global_cgb)
        {
            switch (a)
            {
                case 0xFF4D: 

                    /* wanna switch speed? */
                    if (v & 0x01)
                    {
                        global_double_speed ^= 0x01;

                        /* update new clock */ 
                        cycles_clock = 4194304 << global_double_speed;

                        /* save into memory i'm working at double speed */
                        if (global_double_speed)
                            memory[a] = 0x80;
                        else 
                            memory[a] = 0x00;
                    }
 
                    return;
 
                case 0xFF51: 

                    /* high byte of HDMA source address */
                    mmu.hdma_src_address &= 0xff00;

                    /* lower 4 bits are ignored */
                    mmu.hdma_src_address |= (v & 0xf0);
             
                    break;

                case 0xFF52:

                    /* low byte of HDMA source address */
                    mmu.hdma_src_address &= 0x00ff;

                    /* highet 3 bits are ignored (always 100 binary) */
                    mmu.hdma_src_address |= (v << 8); // ((v & 0x1f) | 0x80) << 8;

                    break;

                case 0xFF53:

                    /* high byte of HDMA source address */
                    mmu.hdma_dst_address &= 0xff00;

                    /* lower 4 bits are ignored */
                    mmu.hdma_dst_address |= (v & 0xf0);

                    break;

                case 0xFF54:

                    /* low byte of HDMA source address */
                    mmu.hdma_dst_address &= 0x00ff;

                    /* highet 3 bits are ignored (always 100 binary) */
                    mmu.hdma_dst_address |= ((v & 0x1f) | 0x80) << 8;

                    break;

                case 0xFF55:

                    /* general (0) or hblank (1) ? */
                    mmu.hdma_transfer_mode = ((v & 0x80) ? 1 : 0);

                    /* calc how many bytes gotta be transferred */
                    uint16_t to_transfer = ((v & 0x7f) + 1) * 0x10;

                    /* general must be done immediately */
                    if (mmu.hdma_transfer_mode == 0)
                    {
                        /* copy right now */
                        if (mmu.vram_idx)
                            memcpy(mmu_addr_vram1() + 
                                   (mmu.hdma_dst_address - 0x8000), 
                                   &memory[mmu.hdma_src_address], to_transfer);
                        else
                            memcpy(mmu_addr_vram0() + 
                                   (mmu.hdma_dst_address - 0x8000), 
                                   &memory[mmu.hdma_src_address], to_transfer);

                        /* reset to_transfer var */
                        mmu.hdma_to_transfer = 0;
                    }
                    else
                         mmu.hdma_to_transfer = to_transfer;
 
                    break;
            }


        }

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
unsigned int mmu_read_16(uint16_t a)
{
    /* 16 bit read = +8 cycles */
    cycles_step();
    cycles_step();

    return (memory[a] | (memory[a + 1] << 8));
}

/* write 16 bit block on a memory address */
void mmu_write_16(uint16_t a, uint16_t v)
{
    memory[a] = (uint8_t) (v & 0x00ff);
    memory[a + 1] = (uint8_t) (v >> 8);

    /* 16 bit write = +8 cycles */
    cycles_step();
    cycles_step();
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

void mmu_term()
{
    if (ram)
    {
        free(ram);
        ram = NULL;
    }
}