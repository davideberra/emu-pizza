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

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>
#include <sys/stat.h>

#include "global.h"
#include "mmu.h"

/* buffer big enough to contain the largest possible ROM */
uint8_t rom[2 << 24];

/* battery backed RAM & RTC*/
char file_sav[1024];
char file_rtc[1024];

/* internal use prototype */
int __mkdirp (char *path, mode_t omode);


/* guess what              */
/* return values           */
/* 0: OK                   */
/* 1: Can't open/read file */
/* 2: Unknown cartridge    */

char cartridge_load(char *file_gb) {
    FILE *fp;
    int i;

    /* open ROM file */
    if ((fp = fopen(file_gb, "r")) == NULL) 
        return 1;

    /* read all the content into rom buffer */
    size_t sz = fread(rom, 1, (2 << 24), fp);

    /* check for errors   */
    if (sz < 1) 
        return 1;

    /* close */
    fclose(fp);
 
    /* gameboy color? */
    if (rom[0x143] == 0xC0)
    {
        printf("Gameboy Color cartridge\n");
        global_cgb = 1;
    }
    else
    {
        printf("Gameboy Classic cartridge\n");
        global_cgb = 0;
    }

    /* get cartridge infos */
    uint8_t mbc = rom[0x147];

    printf("Cartridge code: %02x\n", mbc);

    switch (mbc)
    {
        case 0x00: printf("ROM ONLY\n"); break;
        case 0x01: printf("MBC1\n"); break;
        case 0x02: printf("MBC1 + RAM\n"); break;
        case 0x03: printf("MBC1 + RAM + BATTERY\n"); break;
        case 0x05: printf("MBC2\n"); break;
        case 0x06: mmu_init_ram(512); printf("MBC2 + BATTERY\n"); break;
        case 0x10: printf("MBC3 + TIMER + RAM + BATTERY\n"); break;
        case 0x13: printf("MBC3 + RAM + BATTERY\n"); break;
        case 0x19: printf("MBC5\n"); break;
        case 0x1B: printf("MBC5 + RAM + BATTERY\n"); break;
        case 0x1C: printf("MBC5 + RUMBLE\n"); break;
        case 0x1E: printf("MBC5 + RUMBLE + RAM + BATTERY\n"); break;

        default: printf("Unknown cartridge type: %02x\n", mbc);
                 return 2;
    }

    /* title */
    for (i=0x134; i<0x143; i++)
        if (rom[i] > 0x40 && rom[i] < 0x5B)
            printf("%c", rom[i]);

    printf("\n");

    /* get ROM banks */
    uint8_t byte = rom[0x148];

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
        case 0x52: printf("72 banks\n"); break;
        case 0x53: printf("80 banks\n"); break;
        case 0x54: printf("96 banks\n"); break;
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
        case 0x02: 
                   /* MBC5 got bigger values */
                   if (mbc >= 0x19 && mbc <= 0x1E)
                   {
                       mmu_init_ram(1 << 16); 
                       printf("64 kB\n"); 
                   }
                   else
                   {
                       mmu_init_ram(1 << 13); 
                       printf("8 kB\n"); 
                   }
                   break;
        case 0x03: mmu_init_ram(1 << 15); printf("32 kB\n"); break;
        case 0x04: mmu_init_ram(1 << 17); printf("128 kB\n"); break;
        case 0x05: mmu_init_ram(1 << 16); printf("64 kB\n"); break;
    }

    /* save base name of the rom */
    strlcpy(global_rom_name, basename(file_gb), 256);

    /* build file.sav */
    snprintf(file_sav, sizeof(file_sav), "%s/%s.sav", 
                                         global_save_folder, global_rom_name);

    /* build file.rtc */
    snprintf(file_rtc, sizeof(file_rtc), "%s/%s.rtc", 
                                         global_save_folder, global_rom_name);

    /* restore saved RAM if it's the case */
    mmu_restore_ram(file_sav);

    /* restore saved RTC if it's the case */
    mmu_restore_rtc(file_rtc);

    /* load FULL ROM at 0x0000 address of system memory */
    mmu_load_cartridge(rom, sz);

    return 0; 
}

void cartridge_term()
{
    /* save persistent data (battery backed RAM and RTC clock) */
    mmu_save_ram(file_sav);
    mmu_save_rtc(file_rtc);
}
