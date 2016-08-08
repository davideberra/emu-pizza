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

#include <stdint.h>

/* functions prototypes */
void static inline          *mmu_addr(uint16_t a);
void                         mmu_step(uint8_t t);
uint8_t static inline        mmu_read_no_cyc(uint16_t a);
uint8_t static inline        mmu_read(uint16_t a);
void static inline           mmu_write_no_cyc(uint16_t a, uint8_t v);
void static inline           mmu_write(uint16_t a, uint8_t v);
unsigned int static inline   mmu_read_16(uint16_t a);
void static inline           mmu_write_16(uint16_t a, uint16_t v);
void                         mmu_save_ram(char *fn);
void                         mmu_restore_ram(char *fn);

#endif
