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
#include "subsystem/gameboy/timer.h"
#include "subsystem/gameboy/sound.h"
#include "subsystem/gameboy/mmu.h"
#include "subsystem/gameboy/cycles.h"
#include "subsystem/gameboy/gpu.h"
#include "subsystem/gameboy/globals.h"
#include "subsystem/gameboy/input.h"
#include "subsystem/gameboy/serial.h"
#include "cpu/z80_gameboy_regs.h"
#include "cpu/z80_gameboy.h"


uint8_t bios[] = {
    0x31, 0xFE, 0xFF, 0xAF, 0x21, 0xFF, 0x9F, 0x32, 
    0xCB, 0x7C, 0x20, 0xFB, 0x21, 0x26, 0xFF, 0x0E, 
    0x11, 0x3E, 0x80, 0x32, 0xE2, 0x0C, 0x3E, 0xF3, 
    0xE2, 0x32, 0x3E, 0x77, 0x77, 0x3E, 0xFC, 0xE0,
    0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1A, 
    0xCD, 0x95, 0x00, 0xCD, 0x96, 0x00, 0x13, 0x7B,
    0xFE, 0x34, 0x20, 0xF3, 0x11, 0xD8, 0x00, 0x06, 
    0x08, 0x1A, 0x13, 0x22, 0x23, 0x05, 0x20, 0xF9,
    0x3E, 0x19, 0xEA, 0x10, 0x99, 0x21, 0x2F, 0x99, 
    0x0E, 0x0C, 0x3D, 0x28, 0x08, 0x32, 0x0D, 0x20,
    0xF9, 0x2E, 0x0F, 0x18, 0xF3, 0x67, 0x3E, 0x64, 
    0x57, 0xE0, 0x42, 0x3E, 0x91, 0xE0, 0x40, 0x04,
    0x1E, 0x02, 0x0E, 0x0C, 0xF0, 0x44, 0xFE, 0x90, 
    0x20, 0xFA, 0x0D, 0x20, 0xF7, 0x1D, 0x20, 0xF2,
    0x0E, 0x13, 0x24, 0x7C, 0x1E, 0x83, 0xFE, 0x62, 
    0x28, 0x06, 0x1E, 0xC1, 0xFE, 0x64, 0x20, 0x06,
    0x7B, 0xE2, 0x0C, 0x3E, 0x87, 0xF2, 0xF0, 0x42, 
    0x90, 0xE0, 0x42, 0x15, 0x20, 0xD2, 0x05, 0x20,
    0x4F, 0x16, 0x20, 0x18, 0xCB, 0x4F, 0x06, 0x04, 
    0xC5, 0xCB, 0x11, 0x17, 0xC1, 0xCB, 0x11, 0x17,
    0x05, 0x20, 0xF5, 0x22, 0x23, 0x22, 0x23, 0xC9, 
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D, 
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
    0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E, 
    0x3c, 0x42, 0xB9, 0xA5, 0xB9, 0xA5, 0x42, 0x4C,
    0x21, 0x04, 0x01, 0x11, 0xA8, 0x00, 0x1A, 0x13, 
    0xBE, 0x20, 0xFE, 0x23, 0x7D, 0xFE, 0x34, 0x20,
    0xF5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 
    0xFB, 0x86, 0x20, 0xFE, 0x3E, 0x01, 0xE0, 0x50
};


/* SDL keyboard state reference  */
const Uint8 *kb_state; 


/* Gameboy Z80 DAA instruction */
void static __always_inline gameboy_z80_daa()
{
    unsigned int a = state.a;
    uint8_t al = state.a & 0x0f;

    if (state.flags.n)
    {
        if (state.flags.ac)
            a = (a - 6) & 0xFF;

        if (state.flags.cy)
            a -= 0x60; 
    }
    else
    {
        if (al > 9 || state.flags.ac)
        {
            state.flags.ac = (al > 9);
            a += 6;
        }

        if (state.flags.cy || ((a & 0x1f0) > 0x90))
            a += 0x60;
    }

    if (a & 0x0100) state.flags.cy = 1;

    /* set computer A value */
    state.a = a & 0xff;

    state.flags.ac = 0;
    state.flags.z = (state.a == 0);

    return;
}


/* Gameboy Z80 different instructions */
static __always_inline uint8_t gameboy_z80_execute(uint8_t op)
{
    uint8_t b = 1;
    uint8_t byte = 1;
    uint8_t byte2 = 1;
    unsigned int result;

    /* override default Z80 behaviour for certain opcodes */
    switch (op)
    {
        /* RLCA      */
        case 0x07: z80_rla(&state.a, 1);
                   
                   /* slightly different behaviour compared to z80 */
                   state.flags.n  = 0;
                   state.flags.z  = 0;
                   state.flags.ac = 0;

                   break;

        /* LD  (NN),SP     */
        case 0x08: mmu_write_16(ADDR, state.sp);
                   b = 3;
                   break;

        /* RRC       */
        case 0x0F: z80_rra(&state.a, 1);
                   
                   /* slightly different behaviour compared to z80 */
                   state.flags.n  = 0;
                   state.flags.z  = 0;
                   state.flags.ac = 0;

                   break;

        /* STOP */
        case 0x10: b = 2; 
                   break;

        /* RLA       */
        case 0x17: z80_rla(&state.a, 0);

                   /* slightly different behaviour compared to z80 */
                   state.flags.n  = 0;
                   state.flags.z  = 0;
                   state.flags.ac = 0;

                   break;

        /* RRA       */
        case 0x1F: z80_rra(&state.a, 0);

                   /* slightly different behaviour compared to z80 */
                   state.flags.n  = 0;
                   state.flags.z  = 0;
                   state.flags.ac = 0;

                   break;

        /* LDI (HL), A     */
        case 0x22: mmu_write(*state.hl, state.a);
                   (*state.hl)++;
                   break;

        /* DAA       */
        case 0x27: gameboy_z80_daa(); 
                   break;

        /* LDI  A,(HL)     */ 
        case 0x2A: state.a = mmu_read(*state.hl);
                   (*state.hl)++;
                   break;

        /* LDD (HL), A     */
        case 0x32: mmu_write(*state.hl, state.a);
                   (*state.hl)--;
                   break;

        /* LDD  A,(HL)     */ 
        case 0x3A: state.a = mmu_read(*state.hl);
                   (*state.hl)--;
                   break;

        /* CCF      */
        case 0x3F: state.flags.ac = 0;
                   state.flags.cy = !state.flags.cy;
                   state.flags.n  = 0;

                   break;

        /* not present on Gameboy Z80 */
        case 0xD3: break;

        /* RETI            */
        case 0xD9: state.int_enable = 1;
                   return z80_ret();

        /* not present on Gameboy Z80 */
        case 0xDB:
        case 0xDD: break;

        /* LD   (FF00+N),A */
        case 0xE0: mmu_write(0xFF00 + mmu_read(state.pc + 1), state.a);
                   b = 2;
                   break;

        /* LD   (FF00+C),A */
        case 0xE2: mmu_write(0xFF00 + state.c, state.a);
                   break;

        /* not present on Gameboy Z80 */
        case 0xE3:
        case 0xE4: break;

        /* ADD  SP,dd      */
        case 0xE8: byte = mmu_read(state.pc + 1);
                   byte2 = (uint8_t) (state.sp & 0x00ff);
                   result = byte2 + byte; 

                   state.flags.z = 0;
                   state.flags.n = 0;

                   state.flags.cy = (result > 0xff);

                   /* add 8 cycles */
                   cycles_step(8);

                   /* calc xor for AC  */
                   z80_set_flags_ac(byte2, byte, result);

                   /* set sp */
                   state.sp += (char) byte; // result & 0xffff;

                   b = 2;
                   break;

        /* LD  (NN),A */
        case 0xEA: mmu_write(ADDR, state.a);
                   b = 3; 
                   break;

        /* not present on Gameboy Z80 */
        case 0xEB:
        case 0xEC:
        case 0xED: break;

        /* LD  A,(FF00+N) */
        case 0xF0: state.a = mmu_read(0xFF00 + mmu_read(state.pc + 1));
                   b = 2;
                   break;

        /* LD  A,(FF00+C) */
        case 0xF2: state.a = mmu_read(0xFF00 + state.c);
                   break;

        /* not present on Gameboy Z80 */
        case 0xF4: break;

        /* LD  HL,SP+dd   */
        case 0xF8: byte = mmu_read(state.pc + 1);
                   byte2 = (uint8_t) (state.sp & 0x00ff);
                   result = byte2 + byte;

                   state.flags.z = 0;
                   state.flags.n = 0;

                   state.flags.cy = (result > 0xff);

                   /* add 4 cycles */
                   cycles_step(4);

                   /* calc xor for AC  */
                   z80_set_flags_ac(byte2, byte, result);

                   /* set sp */
                   *state.hl = state.sp + (char) byte; // result & 0xffff;

                   b = 2;
                   break;

        /* LD  A, (NN)    */
        case 0xFA: state.a = mmu_read(ADDR);
                   b = 3;
                   break;

        /* not present on Gameboy Z80 */
        case 0xFC: 
        case 0xFD: break;

        case 0xCB: return 1;
                   /* don't add cycles! it's just a test to know if it's a GB OP */
                   byte = mmu_read_no_cyc(state.pc + 1);

                   if ((byte & 0xf8) != 0x30)
                       return 1;

                   b = 2;

                   /* add 4 cycles */
                   cycles_step(4);

                   switch (byte & 0x37)
                   {
                       /* SWAP B */ 
                       case 0x30: byte = state.b;
                                  state.b = ((byte & 0xf0) >> 4) | 
                                            ((byte & 0x0f) << 4);
                                  break;

                       /* SWAP C */ 
                       case 0x31: byte = state.c;
                                  state.c = ((byte & 0xf0) >> 4) | 
                                            ((byte & 0x0f) << 4);
                                  break;

                       /* SWAP D */ 
                       case 0x32: byte = state.d;
                                  state.d = ((byte & 0xf0) >> 4) | 
                                            ((byte & 0x0f) << 4);
                                  break;

                       /* SWAP E */ 
                       case 0x33: byte = state.e;
                                  state.e = ((byte & 0xf0) >> 4) | 
                                            ((byte & 0x0f) << 4);
                                  break;

                       /* SWAP H */ 
                       case 0x34: byte = state.h;
                                  state.h = ((byte & 0xf0) >> 4) | 
                                            ((byte & 0x0f) << 4);
                                  break;

                       /* SWAP L */ 
                       case 0x35: byte = state.l;
                                  state.l = ((byte & 0xf0) >> 4) | 
                                            ((byte & 0x0f) << 4);
                                  break;

                       /* SWAP *HL */ 
                       case 0x36: byte = mmu_read(*state.hl);
                                  mmu_write(*state.hl, ((byte & 0xf0) >> 4) | 
                                                       ((byte & 0x0f) << 4));
                                  break;

                       /* SWAP A */ 
                       case 0x37: 
                                  byte = state.a;
                                  state.a = ((byte & 0xf0) >> 4) | 
                                            ((byte & 0x0f) << 4);
                                  break;

                   }

                   /* swap functions set Z flags */
                   state.flags.z = (byte == 0x00);
                   
                   /* reset all the others */
                   state.flags.ac = 0;
                   state.flags.cy = 0;
                   state.flags.n  = 0;

                   break;

        default:
                 /* not a Z80 GB op */
                 return 1;
    } 

    state.pc += b;

    return 0;
}

/* entry point for Gameboy emulaton */
void gameboy_start(char *file_gb, uint8_t *rom, size_t size)
{
    uint8_t   op;
    uint8_t   byte;
    char      file_sav[1024];
    int       i;
    uint32_t  benchmark_cnt = 0;
    SDL_Event e;

    global_benchmark = 0;

    /* init z80 */
    z80_init(); 

    /* init input */
    input_init();

    /* get cartridge infos */
    uint8_t mbc = rom[0x147];

    printf("Cartridge code: %02x\n", mbc);

    switch (mbc)
    {
        case 0x00: printf("ROM ONLY\n"); break;
        case 0x01: printf("ROM + MBC1\n"); break;
        case 0x02: printf("ROM + MBC1 + RAM\n"); break;
        case 0x03: printf("ROM + MBC1 + RAM + BATTERY\n"); break;
        case 0x05: printf("ROM + MBC2\n"); break;
        case 0x06: mmu_init_ram(512); printf("ROM + MBC2 + BATTERY\n"); break;

        default: printf("Unknown cartridge type: %02x\n", mbc);
                 return;
    }

    /* title */
    for (i=0x134; i<0x143; i++)
        if (rom[i] > 0x40 && rom[i] < 0x5B)
            printf("%c", rom[i]);

    printf("\n");

    /* get ROM banks */
    byte = rom[0x148];

    printf("ROM: ");

    switch (byte)
    {
        case 0x00: printf("0 banks\n"); break;
        case 0x01: printf("4 banks\n"); break;
        case 0x02: printf("8 banks\n"); break;
        case 0x03: printf("16 banks\n"); break;
        case 0x04: printf("32 banks\n"); break;
        case 0x05: printf("64 banks\n"); break;
        case 0x06: printf("128 banks\n"); break;
        case 0x07: printf("256 banks\n"); break;
    }

    /* init MMU */
    mmu_init(mbc, byte);

    /* get RAM banks */
    byte = rom[0x149];

    printf("RAM: ");

    switch (byte)
    {
        case 0x00: printf("NO RAM\n"); break;
        case 0x01: mmu_init_ram(1 << 11); printf("2 kB\n"); break;
        case 0x02: mmu_init_ram(1 << 13); printf("8 kB\n"); break;
        case 0x03: mmu_init_ram(1 << 15); printf("32 kB\n"); break;
        case 0x04: printf("128 kB\n"); break;
        case 0x05: printf("64 kB\n"); break;
    }

    /* load BIOS at 0x0000 address of system memory */
    mmu_load(bios, 0x100, 0x0000);

    /* load ROM at 0x0100 address of system memory */
    mmu_load(&rom[0x0100], 0x100, 0x0100);

    /* reset SP */
    state.sp = 0x0000;

    /* reset registers */
    state.a = 0x00;
    state.b = 0x00;
    state.c = 0x00;
    state.d = 0x00;
    state.e = 0x00;
    state.h = 0x00;
    state.l = 0x00;

    /* reset flags */
    *state.f = 0x00;

    /* init GPU */
    gpu_init();

    /* init timer */
    timer_init();

    /* init serial */
    serial_init();

    /* init sound (this will start audio thread) */
    sound_init();

    /* init cycles syncronizer */
    cycles_init();

    /* get interrupt flags and interrupt enables */
    uint8_t *int_e;
    uint8_t *int_f;

    /* pointers to memory location of interrupt enables/flags */
    int_e = mmu_addr(0xFFFF);
    int_f = mmu_addr(0xFF0F);

    /* build file.sav */
    snprintf(file_sav, sizeof(file_sav), "%s.sav", file_gb);

    /* restore saved RAM if it's the case */
    mmu_restore_ram(file_sav);

    /* start from 0x0000 and execute BIOS */
    while (0 && state.pc != 0x100)
    {
        /* get op */
        op = mmu_read(state.pc);

        /* override classic Z80 instruction with GB Z80 version */
        // if (gameboy_z80_execute(op) != 0)
            z80_execute(op);

        /* keep low bits always set to zero */
        *state.f &= 0xf0;
    }

    printf("BIOS LOADED\n");

    /* for (i=0xFF00; i<=0xFFFF; i++)
        printf("REGISTRO %04X - VALORE %02X\n", i, mmu_read_no_cyc(i));

    return;*/

    /* load FULL ROM at 0x0000 address of system memory */
    mmu_load_cartridge(rom, size);

    /* reset to default values */
    mmu_write_no_cyc(0xFF05, 0x00);
    mmu_write_no_cyc(0xFF06, 0x00); 
    mmu_write_no_cyc(0xFF07, 0x00);
    mmu_write_no_cyc(0xFF10, 0x80); 
    mmu_write_no_cyc(0xFF11, 0xBF); 
    mmu_write_no_cyc(0xFF12, 0xF3); 
    mmu_write_no_cyc(0xFF14, 0xBF);
    mmu_write_no_cyc(0xFF16, 0x3F);
    mmu_write_no_cyc(0xFF17, 0x00); 
    mmu_write_no_cyc(0xFF19, 0xBF); 
    mmu_write_no_cyc(0xFF1A, 0x7F); 
    mmu_write_no_cyc(0xFF1B, 0xFF);
    mmu_write_no_cyc(0xFF1C, 0x9F); 
    mmu_write_no_cyc(0xFF1E, 0xBF);
    mmu_write_no_cyc(0xFF20, 0xFF); 
    mmu_write_no_cyc(0xFF21, 0x00);
    mmu_write_no_cyc(0xFF22, 0x00); 
    mmu_write_no_cyc(0xFF23, 0xBF); 
    mmu_write_no_cyc(0xFF24, 0x77);
    mmu_write_no_cyc(0xFF25, 0xF3); 
    mmu_write_no_cyc(0xFF26, 0xF1);
    mmu_write_no_cyc(0xFF40, 0x91);
    mmu_write_no_cyc(0xFF42, 0x00);
    mmu_write_no_cyc(0xFF43, 0x00);  
    mmu_write_no_cyc(0xFF44, 0x00);  
    mmu_write_no_cyc(0xFF45, 0x00); 
    mmu_write_no_cyc(0xFF47, 0xFC); 
    mmu_write_no_cyc(0xFF48, 0xFF); 
    mmu_write_no_cyc(0xFF49, 0xFF); 
    mmu_write_no_cyc(0xFF4A, 0x10); 
    mmu_write_no_cyc(0xFF4B, 0x00); 
    mmu_write_no_cyc(0xFFFF, 0x00);  

    mmu_write_no_cyc(0xC000, 0x08);

    mmu_write_no_cyc(0xFFFE, 0x69);

    state.a = 0x01;
    state.b = 0x00;
    state.c = 0x13;
    state.d = 0x00;
    state.e = 0xd8;
    state.h = 0x01;
    state.l = 0x4d;
    state.pc = 0x0100;
    state.sp = 0xFFFE;
    *state.f = 0xB0; 
 
    /* run stuff!                                                          */
    /* mechanism is simple.                                                */
    /* 1) execute instruction 2) update cycles counter 3) check interrupts */
    /* and repeat forever                                                  */
    while (!quit)
    {

        if (global_benchmark)
        {
            benchmark_cnt++;

            if (benchmark_cnt == 40000000)
                break;
        }

        /* get op */
        op   = mmu_read(state.pc);

        /* print out CPU state if enabled by debug flag */
        if (global_debug)
        {
            printf("OP: %02x F: %02x PC: %04x:%02x:%02x SP: %04x:%02x:%02x ", 
                                   op, *state.f & 0xd0, state.pc, 
                                   mmu_read_no_cyc(state.pc + 1),
                                   mmu_read_no_cyc(state.pc + 2), state.sp,
                                   mmu_read_no_cyc(state.sp), 
                                   mmu_read_no_cyc(state.sp + 1));

            printf("A: %02x BC: %04x DE: %04x HL: %04x\n", state.a, *state.bc, 
                                   *state.de, *state.hl);
        }

        /* execute instruction by the GB Z80 version */
        z80_execute(op);

        /* keep low bits always set to zero */
        *state.f &= 0xf0;

        /* if last op was Interrupt Enable (0xFB)  */
        /* we need to check for INTR on next cycle */
        if (op == 0xFB)
            continue;

        /* interrupts filtered by enable flags */
        uint8_t int_r = (*int_f & *int_e);

        /* check for interrupts */
        if ((state.int_enable || op == 0x76) && (int_r != 0))
        {
            /* beware of instruction that doesn't move PC! */
            /* like HALT (0x76)                            */
            if (op == 0x76)
            {
                state.pc++;

                if (state.int_enable == 0)
                    continue;
            }

            /* reset int-enable flag, it will be restored after a RETI op */
            state.int_enable = 0;

            if ((int_r & 0x01) == 0x01)
            {
                /* vblank interrupt triggers RST 5 */

                /* reset flag */
                *int_f &= 0xFE;

                /* handle the interrupt */
                z80_intr(0x0040); 
            }
            else if ((int_r & 0x02) == 0x02)
            {
                /* LCD Stat interrupt */

                /* reset flag */
                *int_f &= 0xFD;

                /* handle the interrupt! */
                z80_intr(0x0048); 
            }       
            else if ((int_r & 0x04) == 0x04)
            {
                /* timer interrupt */

                /* reset flag */
                *int_f &= 0xFB;

                /* handle the interrupt! */
                z80_intr(0x0050); 
            } 
            else if ((int_r & 0x08) == 0x08)
            {
                /* serial interrupt */

                /* reset flag */
                *int_f &= 0xF7;

                /* handle the interrupt! */
                z80_intr(0x0058); 
            } 
        }

        /* check it once in a while, otherwise it will slow down way too much */
        if (cycles_cnt % 65536 == 0)
        {
            /* aaaaaaaaaaaaaand finally, check for SDL events */
            while (SDL_PollEvent(&e))
            {
                switch (e.type)
                {
                    case SDL_QUIT:
                        quit = 1;
                        break;
                    case SDL_KEYDOWN:
                        if (e.key.keysym.sym == SDLK_d)
                            global_debug ^= 0x01;
                }
            }  
        }
    }

    mmu_save_ram(file_sav);

    /* terminate all the stuff */
    sound_term();
 
    return; 
}
