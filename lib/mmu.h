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

#ifndef __MMU_HDR__
#define __MMU_HDR__

#include <stdio.h>
#include <stdint.h>

/* functions prototypes */
void         *mmu_addr(uint16_t a);
void         *mmu_addr_vram0();
void         *mmu_addr_vram1();
void          mmu_dump_all();
void          mmu_init(uint8_t c, uint8_t rn);
void          mmu_init_ram(uint32_t c);
void          mmu_load(uint8_t *data, size_t sz, uint16_t a);
void          mmu_load_cartridge(uint8_t *data, size_t sz);
void          mmu_move(uint16_t d, uint16_t s);
void          mmu_step();
uint8_t       mmu_read_no_cyc(uint16_t a);
uint8_t       mmu_read(uint16_t a);
void          mmu_write_no_cyc(uint16_t a, uint8_t v);
void          mmu_write(uint16_t a, uint8_t v);
unsigned int  mmu_read_16(uint16_t a);
void          mmu_write_16(uint16_t a, uint16_t v);
void          mmu_save_ram(char *fn);
void          mmu_save_rtc(char *fn);
void          mmu_restore_ram(char *fn);
void          mmu_restore_rtc(char *fn);
void          mmu_term();

#endif
