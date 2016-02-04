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
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "z80.h"

/* state of the 8080 CPU */
z80_state_t   state;

/* precomputed parity check array */
uint8_t       parity[256];

/* precomputed flags masks */
uint8_t       sz5p3c[1 << 9];
uint8_t       sz5p3c_mask;
uint8_t       sz5p3[1 << 9];
uint8_t       sz5p3_mask;
uint8_t       szc_16[1 << 17];
uint8_t       szc_16_mask;
 
/* macro to access addresses passed as parameters */
#define ADDR (state.memory[state.pc + 1] | \
             (state.memory[state.pc + 2] << 8))

/* macro to access addresses passed as parameters */
#define NN (state.memory[state.pc + 2] | \
           (state.memory[state.pc + 3] << 8))

/* AC flags tables */
int z80_ac_table[] = { 0, 0, 1, 0, 1, 0, 1, 1 };
int z80_sub_ac_table[] = { 1, 0, 0, 0, 1, 1, 1, 0 };

/* dummy value for 0x06 regs resulution table */
uint8_t dummy;

/* Registers table */
uint8_t **regs_dst;    
uint8_t **regs_src;    

/* Z80 or z80? */
char z80 = 0;



/********************************/
/*                              */
/*    UTIL FUNCTIONS SEGMENT    */
/*                              */
/********************************/



/* print out the register and flags state */
void z80_print_state()
{
    printf("*******************************************\n");
    printf("************ 8080 registers ***************\n");
    printf("*******************************************\n");

    printf("A:  %02x\n", state.a);
    printf("B:  %02x C:  %02x\n", state.b, state.c);
    printf("D:  %02x E:  %02x\n", state.d, state.e);
    printf("H:  %02x L:  %02x\n", state.h, state.l);
    printf("\n");
    printf("SP: %04x\n", state.sp);
    printf("PC: %04x\n", state.pc);
    printf("\n");
    printf("Z:  %x   S:  %x   N:  %x\n", state.flags.z, state.flags.s, state.flags.n);
    printf("P:  %x   CY: %x   AC: %x\n", state.flags.p, state.flags.cy,
                                         state.flags.ac);
    printf("U3:  %x  U5:  %x\n", state.flags.u3, state.flags.u5);
}

/* read 16 bit data from a memory addres */
unsigned int inline z80_read_mem_16(unsigned int a)
{
    return (state.memory[a] | (state.memory[a + 1] << 8));
}

/* write 16 bit block on a memory address */
void inline z80_write_mem_16(unsigned int a, unsigned int v)
{
    state.memory[a] = (uint8_t) (v & 0x00ff); 
    state.memory[a + 1] = (uint8_t) (v >> 8);
}

/* calc flags except for AC */
void inline z80_set_flags(unsigned int answer)
{
    (*state.f) &= sz5p3c_mask;
    (*state.f) |= sz5p3c[answer & 0x1ff];

    return;
}

/* calc flags except for AC and P (it's too variable) - 16 bit param */
void inline z80_set_flags_16(unsigned int answer)
{
    (*state.f) &= szc_16_mask;
    (*state.f) |= szc_16[answer & 0x1ffff];

    return;
}

/* calc flags and ac will be set */
void inline z80_set_flags_ac(unsigned int answer, uint8_t ac)
{
    z80_set_flags(answer);
    state.flags.ac = ac;
}

/* calc flags and ac will be set (16 bit) */
void inline z80_set_flags_ac_16(unsigned int answer, uint8_t ac)
{
    z80_set_flags_16(answer);
    state.flags.ac = ac;
}

/* called after an INC or DRC. It sets AC flag but not CY */
void inline z80_set_flags_no_cy(uint8_t answer, uint8_t ac)
{
    (*state.f) &= sz5p3_mask;
    (*state.f) |= sz5p3[answer & 0x1ff];
    state.flags.ac = ac;

    return;
}

/* only for z80 - calculate if there was an overflow during last operation */
char inline z80_calc_overflow(uint8_t a, uint8_t b, unsigned int r)
{
    unsigned int c = a ^ b ^ r;

    c >>= 7;
    c &= 0x03;

    return (c == 0x01 || c == 0x02); 
}

/* same but with 16 bits operands */
char inline z80_calc_overflow_16(unsigned int a, unsigned int b, unsigned int r)
{
    unsigned int c = a ^ b ^ r;

    c >>= 15;
    c &= 0x03;

    return (c == 0x01 || c == 0x02);
}

/* calculate parity array (parity set to 1 if number of ONES is even) */
void z80_calc_parity_array()
{
    int i = 0, j = 0, n = 0;
    uint8_t b;

    for (i=0; i<256; i++)
    {
        b = (uint8_t) i;
     
        n = 0;
   
        for (j=0; j<8; j++)
        {
            if (b & 0x01)
                n++;

            b = b >> 1;
        }        

        if (n % 2 == 0)
            parity[i] = 1;
        else
            parity[i] = 0; 
    }
}

/* calculate sz5p3 flags mask array */
void z80_calc_sz5p3_array()
{
    unsigned int i;
    z80_flags_t f;

    bzero(sz5p3, sizeof(sz5p3));

    /* build the bit mask */
    f.z  = 0;
    f.s  = 0;
    f.p  = 0;
    f.u5 = 0;
    f.u3 = 0;

    f.cy = 1;
    f.ac = 1;
    f.n  = 1;

    /* make it converge in a uint8_t var */
    sz5p3_mask = *((uint8_t *) &f);

    bzero(&f, 1);

    for (i=0; i<512; i++)
    {
        f.z  = ((i & 0xff) == 0);
        f.s  = ((i & 0x80) == 0x80);
        f.p  = parity[i & 0xff];

        f.u3 = ((i & 0x08) != 0);
        f.u5 = ((i & 0x20) != 0);

        sz5p3[i] = *((uint8_t *) &f);
    }
}

/* calculate sz5p3c flags mask array */
void z80_calc_sz5p3c_array()
{
    unsigned int i;
    z80_flags_t f;

    bzero(sz5p3c, sizeof(sz5p3c));

    /* build the bit mask */
    f.z  = 0;
    f.s  = 0;
    f.cy = 0;
    f.p  = 0;
    f.u5 = 0;
    f.u3 = 0;

    f.ac = 1;
    f.n  = 1;

    /* make it converge in a uint8_t var */
    sz5p3c_mask = *((uint8_t *) &f);
   
    bzero(&f, 1);
 
    for (i=0; i<512; i++)
    {
        f.z  = ((i & 0xff) == 0);
        f.s  = ((i & 0x80) == 0x80);
        f.cy = (i > 0xff);
        f.p  = parity[i & 0xff];

        f.u3 = ((i & 0x08) != 0);
        f.u5 = ((i & 0x20) != 0);

        sz5p3c[i] = *((uint8_t *) &f);
    }
}


/* calculate szc flags mask array */
void z80_calc_szc_16_array()
{
    unsigned int i;
    z80_flags_t f;

    bzero(szc_16, sizeof(szc_16));

    /* build the bit mask */
    f.z  = 0;
    f.s  = 0;
    f.cy = 0;

    f.p  = 1;
    f.ac = 1;
    f.n  = 1;
    f.u5 = 1;
    f.u3 = 1;

    /* make it converge in a uint8_t var */
    szc_16_mask = *((uint8_t *) &f);

    bzero(&f, 1);

    for (i=0; i<(1 << 17); i++)
    {
        f.z  = ((i & 0xffff) == 0);
        f.s  = ((i & 0x8000) == 0x8000);
        f.cy = (i > 0xffff);

        szc_16[i] = *((uint8_t *) &f);
    }
}


/********************************/
/*                              */
/*    INSTRUCTIONS SEGMENT      */
/*       ordered by name        */
/*                              */
/********************************/


/* add A register, b parameter and Carry flag, then calculate flags */
void inline z80_adc(uint8_t b)
{
    /* calc result */
    unsigned int result = state.a + b + state.flags.cy;

    /* calc index of half carry table */
    uint8_t  ihct = ((state.a & 0x88) >> 1) | 
                    ((b & 0x88) >> 2) |
                    ((result & 0x88) >> 3);

    /* calc flags */
    z80_set_flags_ac(result, z80_ac_table[ihct & 0x07]); 

    /* not a subtraction */
    state.flags.n = 0;

    /* set overflow flag */
    state.flags.p  = z80_calc_overflow(state.a, b, result);

    /* save result into A register */
    state.a = (uint8_t) result;

    return; 
}

/* add a and b parameters (both 16 bits) and the carry, thencalculate flags */
unsigned int inline z80_adc_16(unsigned int a, unsigned int b)
{
    /* calc result */
    unsigned int result = a + b + state.flags.cy;

    /* set only carry flag */
    state.flags.cy = (result > 0xffff);

    /* calc index of half carry table */
    uint8_t  ihct = ((a & 0x8800) >> 9) |
                    ((b & 0x8800) >> 10) |
                    ((result & 0x8800) >> 11);

    /* not a subtraction */
    state.flags.n = 0;

    /* set basic flags   */
    z80_set_flags_16(result); 
        
    /* set overflow flag */
    state.flags.p  = z80_calc_overflow_16(a, b, result);

    /* set half carry flag */
    state.flags.ac = z80_ac_table[ihct & 0x07];

    /* calc in 2 parts - extract the carry of the lower bytes sum */
    uint8_t  lpcy = ((a & 0xff) + (b & 0xff) > 0xff);
    unsigned int r16 = (a >> 8) + (b >> 8) + lpcy;

    /* save 3rd and 5th bit of the higher bytes addiction */
    state.flags.u3 = ((r16 & 0x08) != 0);
    state.flags.u5 = ((r16 & 0x20) != 0);

    return result; // (unsigned int) result & 0xffff;
}

/* add A register and b parameter and calculate flags */
void inline z80_add(uint8_t b)
{
    /* calc result */
    unsigned int result = state.a + b;

    /* calc index of half carry table */
    uint8_t  ihct = ((state.a & 0x88) >> 1) | 
                    ((b & 0x88) >> 2) |
                    ((result & 0x88) >> 3);

    /* calc flags */
    z80_set_flags_ac(result, z80_ac_table[ihct & 0x07]);

    /* not a subtraction */
    state.flags.n = 0;

    /* set overflow flag */
    state.flags.p  = z80_calc_overflow(state.a, b, result);
    
    /* save result into A register */
    state.a = result; 

    return; 
}

/*  b AND A register and calculate flags */
void inline z80_ana(uint8_t b)
{
    /* calc result */
    uint8_t result = state.a & b;

    /* calc flags */
    z80_set_flags_ac((unsigned int) result, 1);

    /* set n flag */
    state.flags.n = 0;

    /* save result into A register */
    state.a = result;

    return;
}

/* add a and b parameters (both 16 bits), then calculate flags */
unsigned int inline z80_add_16(unsigned int a, unsigned int b)
{
    /* calc result */
    unsigned int result = a + b;

    /* set only carry flag */
    state.flags.cy = (result > 0xffff);

    /* calc index of half carry table */
    uint8_t  ihct = ((a & 0x8800) >> 9) |
                    ((b & 0x8800) >> 10) |
                    ((result & 0x8800) >> 11);

    /* not a subtraction */
    state.flags.n = 0;

    /* set half carry flag */
    state.flags.ac = z80_ac_table[ihct & 0x07];

    /* calc in 2 parts - extract the carry of the lower bytes sum */
    uint8_t  lpcy = ((a & 0xff) + (b & 0xff) > 0xff);
    unsigned int r16 = (a >> 8) + (b >> 8) + lpcy;

    /* save 3rd and 5th bit of the higher bytes addiction */
    state.flags.u3 = ((r16 & 0x08) != 0);
    state.flags.u5 = ((r16 & 0x20) != 0);

    return result; 
}

/* BIT instruction, test pos-th bit and set flags */
void inline z80_bit(uint8_t *v, uint8_t pos, uint8_t muffa)
{
    uint8_t r = *v & (0x01 << pos);

    state.flags.z = (r == 0);
    state.flags.p  = state.flags.z;
    state.flags.ac = 1;
    state.flags.n  = 0;
    state.flags.s  = ((r & 0x80) == 0x80);

    state.flags.u3 = ((muffa & 0x08) != 0);
    state.flags.u5 = ((muffa & 0x20) != 0);

    return;
}

/* push the current PC on the stack and move PC to the function addr */
int inline z80_call(unsigned int addr)
{
    /* move to the next instruction */
    state.pc += 3;

    /* save it into stack */
    z80_write_mem_16(state.sp - 2, state.pc);

    /* update stack pointer */
    state.sp -= 2;

    /* move PC to the called function address */
    state.pc = addr;

    return 0;
}

/* compare b parameter against A register and calculate flags */
void inline z80_cmp(uint8_t b)
{
    /* calc result */ 
    unsigned int result = state.a - b;

    /* calc index of half carry table */ 
    uint8_t  ihct = ((state.a & 0x88) >> 1) |
                    ((b & 0x88) >> 2) | 
                    ((result & 0x88) >> 3);

    /* calc flags */
    z80_set_flags_ac(result, !z80_sub_ac_table[ihct & 0x07]);

    /* it's a subtraction */
    state.flags.n = 1;

    /* calc overflow flag BUT REMEMBER. numbers must be casted as signed types *
     * so 0x80 (that's -128) - 0x01 overflow it!                               */
    state.flags.p  = z80_calc_overflow(state.a, b, result);

    /* also, flag 3 and 5 are taken from operand and not the result */
    state.flags.u3 = ((b & 0x08) != 0);
    state.flags.u5 = ((b & 0x20) != 0);

    return;
}

/* compare b parameter against A register and calculate flags */
void inline z80_cpid(uint8_t b, int8_t add)
{
    /* calc result */
    unsigned int result = state.a - b;

    /* calc index of half carry table */
    uint8_t  ihct = ((state.a & 0x88) >> 1) |
                    ((b & 0x88) >> 2) |
                    ((result & 0x88) >> 3);

    /* calc flags */
    state.flags.z  = ((result & 0xff) == 0);
    state.flags.s  = ((result & 0x80) == 0x80);
    state.flags.ac = !z80_sub_ac_table[ihct & 0x07];

    /* it's a subtraction */
    state.flags.n = 1;

    /* increase (add = +1) or decrease (add = -1) HL */
    *state.hl += add;

    /* decrease BC */
    *state.bc = *state.bc - 1;

    /* calc n as result - half carry flag */
    unsigned int n = result - state.flags.ac;

    /* set p = 1 if BC != 0 */
    state.flags.p  = (*state.bc != 0);

    /* also, flag 3 and 5 are taken from result - ac not the result */
    /* and u5 is taken exceptionally from the bit 1                 */
    state.flags.u3 = ((n & 0x0008) != 0);
    state.flags.u5 = ((n & 0x0002) != 0);

    return;
}

/* DAA instruction... what else? */
void inline z80_daa()
{
    unsigned int a = state.a;
    uint8_t al = state.a & 0x0f;

    if (state.flags.n)
    {
        uint8_t hd = (state.flags.cy || state.a > 0x99); 

        if (al > 9 || state.flags.ac)
        {
            if (al > 5) state.flags.ac = 0;
            a = (a - 6) & 0xff;
        }

        if (hd) a -= 0x160;
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

    /* and its flags */
    z80_set_flags_no_cy(state.a, state.flags.ac);

    return;
}

/* add a and b parameters (both 16 bits) and the carry, thencalculate flags */
unsigned int inline dad_16(unsigned int a, unsigned int b)
{
    /* calc result */
    unsigned int result = a + b;

    /* set carry flag */
    state.flags.cy = (result > 0xffff);

    /* calc index of half carry table */
    uint8_t  ihct = ((a & 0x8800) >> 9) |
                    ((b & 0x8800) >> 10) |
                    ((result & 0x8800) >> 11);

    /* not a subtraction */
    state.flags.n = 0;

    /* set half carry flag */
    state.flags.ac = z80_ac_table[ihct & 0x07];

    /* calc in 2 parts - extract the carry of the lower bytes sum */
    uint8_t  lpcy = ((a & 0xff) + (b & 0xff) > 0xff);
    unsigned int r16 = (a >> 8) + (b >> 8) + lpcy;

    /* save 3rd and 5th bit of the higher bytes addiction */
    state.flags.u3 = ((r16 & 0x08) != 0);
    state.flags.u5 = ((r16 & 0x20) != 0);

    return result; 
}

/* dec the operand and return result increased by one */
uint8_t inline z80_dcr(uint8_t b)
{
    unsigned int result = b - 1;

    /* set basic flags */
    z80_set_flags_no_cy((uint8_t) result & 0xff, ((result & 0x0f) == 0x0f));

    /* i'm a subtraction! */
    state.flags.n  = 1;

    /* calc overflow */
    state.flags.p  = z80_calc_overflow(b, 1, result);

    return result; 
}

/* inc the operand and return result increased by one */
uint8_t inline z80_inr(uint8_t b)
{
    unsigned int result = b + 1;
    
    z80_set_flags_no_cy((uint8_t) result & 0xff, ((result & 0x0f) == 0x00)); 

    /* not a subtraction */
    state.flags.n  = 0;

    /* calc overflow */
    state.flags.p  = z80_calc_overflow(b, 1, result);

    return result; 
}

/* same as call, but save on the stack the current PC instead of next instr */
int inline z80_intr(unsigned int addr)
{
    /* push the current PC into stack */
    z80_write_mem_16(state.sp - 2, state.pc);

    /* update stack pointer */
    state.sp -= 2;

    /* move PC to the called function address */
    state.pc = addr;

    return 0;
}

/* copy (HL) in (DE) and decrease HL, DE and BC */
void inline z80_ldd()
{
    uint8_t byte;

    /* copy! */
    state.memory[*state.de] = state.memory[*state.hl];

    /* get last moved byte and sum A */
    byte = state.memory[*state.de];
    byte += state.a;

    /* u5 flag is bit 1 of last moved byte + A (WTF?) */
    state.flags.u5 = (byte & 0x02) >> 1;

    /* u3 flag is bit 3 of last moved byte + A (WTF?) */
    state.flags.u3 = (byte & 0x08) >> 3;

    /* decrease HL, DE and BC */
    *state.hl = *state.hl - 1;
    *state.de = *state.de - 1;
    *state.bc = *state.bc - 1;

    /* reset negative, half carry and parity flags */
    state.flags.n = 0;
    state.flags.ac = 0;
    state.flags.p = (*state.bc != 0);

    return;
}

/* copy (HL) in (DE) and increase HL and DE. BC is decreased */
void inline z80_ldi()
{
    uint8_t byte;

    /* copy! */
    state.memory[*state.de] = state.memory[*state.hl];

    /* get last moved byte and sum A */
    byte = state.memory[*state.de];
    byte += state.a;

    /* u5 flag is bit 1 of last moved byte + A (WTF?) */
    state.flags.u5 = (byte & 0x02) >> 1;
    
    /* u3 flag is bit 3 of last moved byte + A (WTF?) */
    state.flags.u3 = (byte & 0x08) >> 3;
    
    /* decrease HL, DE and BC */
    *state.hl = *state.hl + 1;
    *state.de = *state.de + 1;
    *state.bc = *state.bc - 1;

    /* reset negative, half carry and parity flags */
    state.flags.n = 0;
    state.flags.ac = 0;
    state.flags.p = (*state.bc != 0);

    return;
}

/* negate register A */
void inline z80_neg()
{
    /* calc result */
    unsigned int result = 0 - state.a;

    /* calc index of half carry table */
    uint8_t  ihct = ((0 & 0x88) >> 1) |
                    ((state.a & 0x88) >> 2) |
                    ((result & 0x88) >> 3);

    /* calc flags */
    z80_set_flags_ac(result, !z80_sub_ac_table[ihct & 0x07]);

    /* not a subtraction */
    state.flags.n = 1;

    /* set overflow flag */
    state.flags.p  = z80_calc_overflow(0, state.a, result);

    /* save result into A register */
    state.a = (uint8_t) result;

    return;
}

/* OR b parameter and A register and calculate flags */
void inline z80_ora(uint8_t b)
{
    /* calc result */ 
    uint8_t result = state.a | b;

    /* calc flags */
    z80_set_flags_ac(result, 0);

    /* reset N */
    state.flags.n = 0;

    /* save result into A register */
    state.a = (uint8_t) result;

    return;
}

/* RES instruction, put a 0 on pos-th bit and set flags */
uint8_t inline z80_res(uint8_t *v, uint8_t pos)
{
    *v &= ~(0x01 << pos);

    return *v;
}

/* pop the return address from the stack and move PC to that address */
int inline z80_ret()
{
    state.pc = z80_read_mem_16(state.sp); 
    state.sp += 2;

    return 0;
}

/* RL (Rotate Left) instruction */
uint8_t inline z80_rl(uint8_t *v, char with_carry)
{
    uint8_t carry;

    /* apply RLC to the memory pointed byte */
    carry = (*v & 0x80) >> 7;
    *v = *v << 1;

    if (with_carry)
        *v |= carry;
    else
        *v |= state.flags.cy;

    /* save carry */
    state.flags.cy = carry;

    /* set the other flags */
    z80_set_flags_no_cy((unsigned int) *v, 0);

    /* reset N */
    state.flags.n = 0;

    return *v;
}

/* RLA instruction */
uint8_t inline z80_rla(uint8_t *v, char with_carry)
{
    uint8_t carry;

    /* apply RLA to the memory pointed byte */
    carry = (*v & 0x80) >> 7;
    *v = *v << 1;

    if (with_carry)
        *v |= carry;
    else
        *v |= state.flags.cy;

    /* set flags */
    state.flags.cy = carry;
    state.flags.n = 0;
    state.flags.ac = 0;

    /* copy bit 3 and 5 of the result */
    state.flags.u3 = ((*v & 0x08) != 0);
    state.flags.u5 = ((*v & 0x20) != 0);

    return *v;
}

/* RLD instruction */
void inline z80_rld()
{
    uint8_t hl = state.memory[*state.hl];

    /* save lowest A 4 bits */
    uint8_t al = state.a & 0x0f;

    /* A lowest bits are overwritten by (HL) highest ones */
    state.a &= 0xf0;
    state.a |= (hl >> 4);

    /* (HL) highest bits are overwritten by (HL) lowest ones */
    hl <<= 4;

    /* finally, (HL) lowest bits are overwritten by A lowest */
    hl &= 0xf0;
    hl |= al;

    /* set (HL) with his new value ((HL) low | A low) */
    state.memory[*state.hl] = hl;

    /* set basic flags */
    z80_set_flags_no_cy(state.a, 0);
/*    state.flags.z  = (state.a == 0);   
    state.flags.s  = ((state.a & 0x80) == 0x80); */

    /* set parity */
//    state.flags.p  = parity[state.a];

    /* n = 0 */
    state.flags.n = 0;
//    state.flags.ac = 0;

    /* copy bit 3 and 5 of the result */
/*    state.flags.u3 = ((state.a & 0x08) != 0);
    state.flags.u5 = ((state.a & 0x20) != 0); */

    return;
}

/* RR instruction */
uint8_t inline z80_rr(uint8_t *v, char with_carry)
{
    uint8_t carry;

    /* apply RRC to the memory pointed byte */
    carry = *v & 0x01;
    *v = (*v >> 1);

	/* 7th bit taken from old bit 0 or from CY */
    if (with_carry)
        *v |= (carry << 7);
    else
        *v |= (state.flags.cy << 7);

    /* save carry */ 
    state.flags.cy = carry;

    /* set the other flags */
    z80_set_flags_no_cy((unsigned int) *v, 0);

    /* reset N */ 
    state.flags.n = 0;

    return *v;
}

/* RRA instruction */
uint8_t inline z80_rra(uint8_t *v, char with_carry)
{
    uint8_t carry;

    /* apply RRC to the memory pointed byte */
    carry = *v & 0x01;
    *v = (*v >> 1); 

	/* 7th bit taken from old bit 0 or from CY */
    if (with_carry)
        *v |= (carry << 7);
    else
        *v |= (state.flags.cy << 7);

    state.flags.cy = carry;

    state.flags.n = 0;
    state.flags.ac = 0;

    /* copy bit 3 and 5 of the result */
    state.flags.u3 = ((*v & 0x08) != 0);
    state.flags.u5 = ((*v & 0x20) != 0);

    return *v;
}

/* RRD instruction */
void inline z80_rrd()
{
    uint8_t hl = state.memory[*state.hl];

    /* save lowest (HL) 4 bits */
    uint8_t hll = hl & 0x0f;

    /* (HL) lowest bits are overwritten by (HL) highest ones */
    hl >>= 4;

    /* (HL) highest bits are overwritten by A lowest ones */
    hl |= ((state.a & 0x0f) << 4);

    /* set (HL) with his new value (A low | (HL) high) */
    state.memory[*state.hl] = hl;
 
    /* finally, A lowest bits are overwritten by (HL) lowest */
    state.a &= 0xf0;
    state.a |= hll;   

    /* set basic flags */
    z80_set_flags_no_cy(state.a, 0);

/*
    state.flags.z  = (state.a == 0);        
    state.flags.s  = ((state.a & 0x80) == 0x80);
*/
    /* set parity */
//    state.flags.p  = parity[state.a];

    /* n and ac = 0 */
    state.flags.n = 0;
//    state.flags.ac = 0;

    /* carry always 1 */
    // state.flags.cy = 1;

    /* copy bit 3 and 5 of the result */
//    state.flags.u3 = ((state.a & 0x08) != 0);
//    state.flags.u5 = ((state.a & 0x20) != 0);

    return;
}

/* subtract b parameter and Carry from A register and calculate flags */
void inline z80_sbc(uint8_t b)
{
    /* calc result */
    unsigned int result = state.a - b - state.flags.cy;

    /* calc index of half carry table */
    uint8_t  ihct = ((state.a & 0x88) >> 1) |
                    ((b & 0x88) >> 2) |
                    ((result & 0x88) >> 3);

    /* calc flags */
    z80_set_flags_ac(result, !z80_sub_ac_table[ihct & 0x07]);

    /* not a subtraction */
    state.flags.n = 1;

    /* set overflow flag */
    state.flags.p  = z80_calc_overflow(state.a, b, result);

    /* save result into A register */
    state.a = (uint8_t) result;

    return;
}

/* same as sbc but in 16 bit flavour */
unsigned int inline z80_sbc_16(unsigned int v1, unsigned int v2)
{
    /* calc result */
    unsigned int result = v1 - v2 - state.flags.cy;

    /* calc flags */
    z80_set_flags_16(result);

    /* calc overflow flag - verify if sign is changed  */
    state.flags.p = (((v2 & 0x8000) != (v1 & 0x8000)) && 
                     ((result & 0x8000) != (v1 & 0x8000)));
  
    /* subtraction */ 
    state.flags.n = 1;

    /* calc */
    unsigned int r16 = (v1 >> 8) - (v2 >> 8);

    /* save 3rd and 5th bit of the higher bytes addiction */
    state.flags.u3 = ((r16 & 0x08) != 0);
    state.flags.u5 = ((r16 & 0x20) != 0);

    return result; 
}

/* SET instruction, put a 1 on pos-th bit and set flags */
uint8_t inline z80_set(uint8_t *v, uint8_t pos)
{
    *v |= (0x01 << pos);

    return *v;
}

/* SL instruction (SLA = v * 2, SLL = v * 2 + 1) */
uint8_t inline z80_sl(uint8_t *v, char one_insertion)
{
    /* apply SL to the memory pointed byte */
    state.flags.cy = (*v & 0x80) >> 7;
    *v = (*v << 1) | one_insertion;

    /* set basic flags */
    z80_set_flags_no_cy((unsigned int) *v, 0);

    /* set parity */ 
    // state.flags.p  = parity[*v];

    /* n = 0 */
    state.flags.n = 0;

    return *v;
}

/* SR instruction (SRA = preserve 8th bit, SRL = discard 8th bit) */
uint8_t inline z80_sr(uint8_t *v, char preserve)
{
    uint8_t bit = 0;

    /* save the bit 0 */
    state.flags.cy = (*v & 0x01);

    /* apply SL to the memory pointed byte */
    if (preserve)
        bit = *v & 0x80;

    /* move 1 pos right and restore highest bit (in case of SRA) */
    *v = (*v >> 1) | bit;

    /* set basic flags */
    z80_set_flags_no_cy((unsigned int) *v, 0);

    /* n = 0 */
    state.flags.n = 0;

    return *v;
}

/* subtract b parameter from A register and calculate flags */
void inline z80_sub(uint8_t b)
{
    /* calc result */
    unsigned int result = state.a - b;

    /* calc index of half carry table */
    uint8_t  ihct = ((state.a & 0x88) >> 1) | 
                    ((b & 0x88) >> 2) |
                    ((result & 0x88) >> 3);

    /* calc flags */
    z80_set_flags_ac(result, !z80_sub_ac_table[ihct & 0x07]);

    /* subtraction */
    state.flags.n = 1;
        
    /* set overflow flag */
    state.flags.p  = z80_calc_overflow(state.a, b, result);

    /* save result into A register */
    state.a = (uint8_t) result;

    return;
}

/* subtract a and b parameters (both 16 bits) and the carry, thencalculate flags */
unsigned int inline z80_sub_16(unsigned int a, unsigned int b, uint8_t cy)
{
    /* calc result */
    unsigned int result = a - b - cy;

    /* set only carry flag */
    state.flags.cy = (result > 0xffff);

    /* calc index of half carry table */
    uint8_t  ihct = ((a & 0x8800) >> 9) |
                    ((b & 0x8800) >> 10) |
                    ((result & 0x8800) >> 11);

    /* it's a subtraction */
    state.flags.n = 1;

    /* calc basic flags */
    z80_set_flags_16(result);

    /* calc overflow */
    state.flags.p  = z80_calc_overflow_16(a, b, result);

    /* set half carry flag */
    state.flags.ac = !z80_sub_ac_table[ihct & 0x07];

    /* calc */
    unsigned int r16 = (a >> 8) - (b >> 8) - cy;

    /* save 3rd and 5th bit of the higher bytes addiction */
    state.flags.u3 = ((r16 & 0x08) != 0);
    state.flags.u5 = ((r16 & 0x20) != 0);

    return result; 
}

/* xor b parameter and A register and calculate flags */
void inline z80_xra(uint8_t b)
{
    /* calc result */
    uint8_t result = state.a ^ b;

    /* calc flags */
    z80_set_flags_ac(result, 0);

    /* always 0 */
    state.flags.cy = 0;
    state.flags.n  = 0;

    /* save result into A register */
    state.a = (uint8_t) result;

    return;
}



/********************************/
/*                              */
/*    INSTRUCTIONS BRANCHES     */
/*                              */
/********************************/


int inline z80_run()
{
    unsigned char code = state.memory[state.pc];

    return z80_execute(code);
}

/* Z80 extended OPs */
int inline inline z80_ext_cb_execute()
{
    int b = 2;

    /* CB family (ROT, BIT, RES, SET)    */
    uint8_t cbfam;

    /* CB operation     */
    uint8_t cbop;

    /* choosen register   */
    uint8_t reg;

    /* get CB code */
    uint8_t code = state.memory[state.pc + 1];

    /* extract family */
    cbfam = code >> 6;

    /* extract involved register */
    reg = code & 0x07;

    /* if reg == 0x06, refresh the pointer */
    if (reg == 0x06)
        regs_src[0x06] = &state.memory[*state.hl]; 

    switch (cbfam)
    {
        /* Rotate Family */
        case 0x00: cbop = code & 0xf8;

                   switch(cbop)
                   {
                       /* RLC REG */
                       case 0x00: z80_rl(regs_src[reg], 1);
                                  break;

                       /* RRC REG */
                       case 0x08: z80_rr(regs_src[reg], 1);
                                  break;

                       /* RL  REG */
                       case 0x10: z80_rl(regs_src[reg], 0);
                                  break;

                       /* RR  REG */
                       case 0x18: z80_rr(regs_src[reg], 0);
                                  break;

                       /* SLA REG */
                       case 0x20: z80_sl(regs_src[reg], 0);
                                  break;

                       /* SRA REG */
                       case 0x28: z80_sr(regs_src[reg], 1);
                                  break;

                       /* SLL REG */
                       case 0x30: z80_sl(regs_src[reg], 1);
                                  break;

                       /* SRL REG */
                       case 0x38: z80_sr(regs_src[reg], 0);
                                  break;

                       default: printf("Unimplemented CB ROT op: %02x\n",
                                       cbop);
                   }
                   break;

        /* BIT Family */
        case 0x01: if (reg == 0x06)
                       z80_bit(regs_src[reg], (code >> 3) & 0x07, (uint8_t) *state.hl);
                   else
                       z80_bit(regs_src[reg], (code >> 3) & 0x07, *regs_src[reg]);
                   break;

        /* RES Family */
        case 0x02: z80_res(regs_src[reg], (code >> 3) & 0x07);
                   break;

        /* SET Family */
        case 0x03: z80_set(regs_src[reg], (code >> 3) & 0x07);
                   break;

        default: printf("Unimplemented CB family: %02x\n",
                        cbfam);

    }

    return b;
}

/* Z80 extended OPs */
int inline z80_ext_dd_execute()
{
    int b = 2;

    /* displacemente byte */
    uint8_t d;
   
    /* DDCB family (ROT, BIT, RES, SET)    */
    uint8_t ddcbfam;
   
    /* DDCB operation     */
    uint8_t ddcbop;
   
    /* choosen register   */
    uint8_t reg;

    /* i already know it's a DD - get the next byte */
    unsigned char code = state.memory[state.pc + 1];

    switch (code)
    {
        /* ADD IX+BC      */
        case 0x09: *state.ix = z80_add_16(*state.ix, *state.bc);
                   break;

        /* ADD IX+DE      */
        case 0x19: *state.ix = z80_add_16(*state.ix, *state.de);
                   break;

        /* LD  IX,NN      */
        case 0x21: *state.ix = NN;
                   b = 4;
                   break;

        /* LD  (NN),IX    */
        case 0x22: z80_write_mem_16(NN, *state.ix);
                   b = 4;
                   break;

        /* INC IX         */
        case 0x23: *state.ix = *state.ix + 1;
                   break;

        /* INC IXH        */
        case 0x24: state.ixh = z80_inr(state.ixh);
                   break;

        /* DEC IXH        */
        case 0x25: state.ixh = z80_dcr(state.ixh);
                   break;

        /* LD  IXH,N      */
        case 0x26: state.ixh = state.memory[state.pc + 2];
                   b = 3;
                   break;

        /* ADD IX+IX      */
        case 0x29: *state.ix = z80_add_16(*state.ix, *state.ix);
                   break;

        /* LD  IX,(NN)    */
        case 0x2A: *state.ix = z80_read_mem_16(NN);
                   b = 4;
                   break;

        /* DEC IX         */
        case 0x2B: (*state.ix)--; // = *state.ix - 1;
                   break;

        /* INC IXL        */
        case 0x2C: state.ixl = z80_inr(state.ixl);
                   break;

        /* DEC IXL        */
        case 0x2D: state.ixl = z80_dcr(state.ixl);
                   break;

        /* LD  IXL,N      */
        case 0x2E: state.ixl = state.memory[state.pc + 2];
                   b = 3;
                   break;

        /* INC (IX+d)     */
        case 0x34: d = state.memory[state.pc + 2];
                   state.memory[*state.ix + d] = 
                       z80_inr(state.memory[*state.ix + d]);                   
                   b = 3;
                   break;
 
        /* DEC (IX+d)     */
        case 0x35: d = state.memory[state.pc + 2];
                   state.memory[*state.ix + d] = 
                       z80_dcr(state.memory[*state.ix + d]);                   
                   b = 3;
                   break;

        /* LD (IX+d),N    */
        case 0x36: d = state.memory[state.pc + 2];
                   state.memory[*state.ix + d] = state.memory[state.pc + 3];
                   b = 4;
                   break;
 
        /* ADD IX+SP      */
        case 0x39: *state.ix = z80_add_16(*state.ix, state.sp);
                   break;

        /* LD  B,(IX+d)   */
        case 0x46: d = state.memory[state.pc + 2];
                   state.b = state.memory[*state.ix + d];
                   b = 3;
                   break;

        /* LD  C,(IX+d)   */
        case 0x4E: d = state.memory[state.pc + 2];
                   state.c = state.memory[*state.ix + d];
                   b = 3;
                   break;

        /* LD  D,(IX+d)   */
        case 0x56: d = state.memory[state.pc + 2];
                   state.d = state.memory[*state.ix + d];
                   b = 3;
                   break;

        /* LD  E,(IX+d)   */
        case 0x5E: d = state.memory[state.pc + 2];
                   state.e = state.memory[*state.ix + d];
                   b = 3;
                   break;

        /* LD  IXH, B     */
        case 0x60: state.ixh = state.b;
                   break;

        /* LD  IXH, C     */
        case 0x61: state.ixh = state.c;
                   break;

        /* LD  IXH, D     */
        case 0x62: state.ixh = state.d;
                   break;

        /* LD  IXH, E     */
        case 0x63: state.ixh = state.e;
                   break;

        /* LD  IXH, H     */
        case 0x64: state.ixh = state.h;
                   break;

        /* LD  IXH, L     */
        case 0x65: state.ixh = state.l;
                   break;

        /* LD  H,(IX+d)   */
        case 0x66: d = state.memory[state.pc + 2];
                   state.h = state.memory[*state.ix + d];
                   b = 3;
                   break;

        /* LD  IXH, A     */
        case 0x67: state.ixh = state.a;
                   break;

        /* LD  IXL, C     */
        case 0x68: state.ixl = state.b;
                   break;

        /* LD  IXL, C     */
        case 0x69: state.ixl = state.c;
                   break;

        /* LD  IXL, D     */
        case 0x6A: state.ixl = state.d;
                   break;

        /* LD  IXL, E     */
        case 0x6B: state.ixl = state.e;
                   break;

        /* LD  IXL, H     */
        case 0x6C: state.ixl = state.ixh;
                   break;

        /* LD  IXL, IXL   */
        case 0x6D: state.ixl = state.ixl;
                   break;

        /* LD  L,(IX+d)   */
        case 0x6E: d = state.memory[state.pc + 2];
                   state.l = state.memory[*state.ix + d];
                   b = 3;
                   break;

        /* LD  IXL, A     */
        case 0x6F: state.ixl = state.a;
                   break;

        /* MOV (IX+d), B  */
        case 0x70: d = state.memory[state.pc + 2];
                   state.memory[*state.ix + d] = state.b;
                   b = 3;
                   break;

        /* MOV (IX+d), C  */
        case 0x71: d = state.memory[state.pc + 2];
                   state.memory[*state.ix + d] = state.c;
                   b = 3;
                   break;

        /* MOV (IX+d), D  */
        case 0x72: d = state.memory[state.pc + 2];
                   state.memory[*state.ix + d] = state.d;
                   b = 3;
                   break;

        /* MOV (IX+d), E  */
        case 0x73: d = state.memory[state.pc + 2];
                   state.memory[*state.ix + d] = state.e;
                   b = 3;
                   break;

        /* MOV (IX+d), H  */
        case 0x74: d = state.memory[state.pc + 2];
                   state.memory[*state.ix + d] = state.h;
                   b = 3;
                   break;

        /* MOV (IX+d), L  */
        case 0x75: d = state.memory[state.pc + 2];
                   state.memory[*state.ix + d] = state.l;
                   b = 3;
                   break;

        /* MOV (IX+d), A  */
        case 0x77: d = state.memory[state.pc + 2];
                   state.memory[*state.ix + d] = state.a;
                   b = 3;
                   break;

        /* LD  A,(IX+d)   */
        case 0x7E: d = state.memory[state.pc + 2];
                   state.a = state.memory[*state.ix + d];
                   b = 3;
                   break;

        /* ADD IXH        */
        case 0x84: z80_add((uint8_t) (*state.ix >> 8));
                   break;

        /* ADD IXL        */
        case 0x85: z80_add(state.ixl);
                   break;

        /* ADD (IX+d)     */
        case 0x86: d = state.memory[state.pc + 2];
                   z80_add(state.memory[*state.ix + d]);
                   b = 3;
                   break;

        /* ADC  IXH      */
        case 0x8C: z80_adc((uint8_t) (*state.ix >> 8));
                   break;

        /* ADC  IXL      */
        case 0x8D: z80_adc(state.ixl);
                   break;

        /* ADC (IX+d)    */
        case 0x8E: d = state.memory[state.pc + 2];
                   z80_adc(state.memory[*state.ix + d]);
                   b = 3;
                   break;

        /* SUB IXH        */
        case 0x94: z80_sub(state.ixh);
                   break;

        /* SUB IXL        */
        case 0x95: z80_sub(state.ixl);
                   break;

        /* SUB (IX+d)     */
        case 0x96: d = state.memory[state.pc + 2];
                   z80_sub(state.memory[*state.ix + d]);
                   b = 3;
                   break;

        /* SBC  IXH       */
        case 0x9C: z80_sbc((uint8_t) (*state.ix >> 8));
                   break;

        /* SBC  IXL       */
        case 0x9D: z80_sbc(state.ixl);
                   break;

        /* SBC (IX+d)     */
        case 0x9E: d = state.memory[state.pc + 2];
                   z80_sbc(state.memory[*state.ix + d]);
                   b = 3;
                   break;

        /* ANA  IXH       */
        case 0xA4: z80_ana((uint8_t) (*state.ix >> 8));
                   break;

        /* ANA  IXL       */
        case 0xA5: z80_ana(state.ixl);
                   break;

        /* ANA (IX+d)     */
        case 0xA6: d = state.memory[state.pc + 2];
                   z80_ana(state.memory[*state.ix + d]);
                   b = 3;
                   break;

        /* XRA  IXH       */
        case 0xAC: z80_xra((uint8_t) (*state.ix >> 8));
                   break;

        /* XRA  IXL       */
        case 0xAD: z80_xra(state.ixl);
                   break;

        /* XRA (IX+d)     */
        case 0xAE: d = state.memory[state.pc + 2];
                   z80_xra(state.memory[*state.ix + d]);
                   b = 3;
                   break;

        /* OR    IXH      */
        case 0xB4: z80_ora((uint8_t) (*state.ix >> 8));
                   break;

        /* OR    IXL      */
        case 0xB5: z80_ora(state.ixl);
                   break;

        /* ORA (IX+d)     */
        case 0xB6: d = state.memory[state.pc + 2];
                   z80_ora(state.memory[*state.ix + d]);
                   b = 3;
                   break;

        /* CMP   IXH      */
        case 0xBC: z80_cmp((uint8_t) (*state.ix >> 8));
                   break;

        /* CMP   IXL      */
        case 0xBD: z80_cmp(state.ixl);
                   break;

        /* CMP (IX+d)     */
        case 0xBE: d = state.memory[state.pc + 2];
                   z80_cmp(state.memory[*state.ix + d]);
                   b = 3;
                   break;

        /* DDCB Operation */
        case 0xCB: d = state.memory[state.pc + 2];

                   /* bit 6-7 reprezent family of DDCB op */
                   ddcbfam = state.memory[state.pc + 3] >> 6;

                   /* bit 0-2 reprezent output register */
                   reg = state.memory[state.pc + 3] & 0x07;

                   switch (ddcbfam)
                   {
                       /* Rotate family */
                       case 0x00: 
 
                           /* bit 3-5 reprezent ROT operation */
                           ddcbop = state.memory[state.pc + 3] & 0x38;
                                  
                           switch(ddcbop)
                           {
                               /* RLC (IX+d) REG */
                               case 0x00: *regs_dst[reg] = 
                                          z80_rl(&state.memory[*state.ix + d], 1);
                                          break;

                               /* RRC (IX+d) REG */
                               case 0x08: *regs_dst[reg] = 
                                          z80_rr(&state.memory[*state.ix + d], 1);
                                          break;

                               /* RL  (IX+d) REG */
                               case 0x10: *regs_dst[reg] = 
                                          z80_rl(&state.memory[*state.ix + d], 0);
                                          break;

                               /* RR  (IX+d) REG */
                               case 0x18: *regs_dst[reg] = 
                                          z80_rr(&state.memory[*state.ix + d], 0);
                                          break;

                               /* SLA (IX+d) REG */
                               case 0x20: *regs_dst[reg] = 
                                          z80_sl(&state.memory[*state.ix + d], 0);
                                          break;

                               /* SRA (IX+d) REG */
                               case 0x28: *regs_dst[reg] = 
                                          z80_sr(&state.memory[*state.ix + d], 1);
                                          break;

                               /* SLL (IX+d) REG */
                               case 0x30: *regs_dst[reg] = 
                                          z80_sl(&state.memory[*state.ix + d], 1);
                                          break;

                               /* SRL (IX+d) REG */
                               case 0x38: *regs_dst[reg] = 
                                          z80_sr(&state.memory[*state.ix + d], 0);
                                          break;

/*                               default: printf("Unimplemented DDCB family: %02x"
                                               " - MEM: %02x %02x\n",      
                                               ddcbfam,
                                               state.memory[state.pc + 2],
                                               state.memory[state.pc + 3]); */
                           }
                           break;

                       /* BIT Family */
                       case 0x01: z80_bit(&state.memory[*state.ix + d], 
                                      (state.memory[state.pc + 3] >> 3) & 0x07, 
                                      d);
                                  break;
                       
                       /* RES Family */
                       case 0x02: *regs_dst[reg] = 
                                      z80_res(&state.memory[*state.ix + d], 
                                      (state.memory[state.pc + 3] >> 3) & 0x07);
                                  break;
                       
                       /* SET Family */
                       case 0x03: *regs_dst[reg] = 
                                      z80_set(&state.memory[*state.ix + d], 
                                      (state.memory[state.pc + 3] >> 3) & 0x07);
                                  break;
                       
/*                       default: printf("Unimplemented DDCB family: %02x"
                                       " - MEM: %02x %02x\n", 
                                       ddcbfam,
                                       state.memory[state.pc + 2],
                                       state.memory[state.pc + 3]); */
                     
                   }

                   b = 4;
                   break;

        /* POP   IX     */       
        case 0xE1: *state.ix = z80_read_mem_16(state.sp); 
                   state.sp += 2;
                   break;

        /* PUSH IX        */
        case 0xE5: z80_write_mem_16(state.sp - 2, *state.ix);
                   state.sp -= 2;
                   break;

        default: // printf("Unimplemented Z80 DD OP code: %02x\n", code);
                 state.skip_cycle = 1;
                 b = 1; 
    } 

    return b;
}

/* Z80 extended OPs */
int inline z80_ext_ed_execute()
{
    int b = 2;

    /* i already know it's an ED - get the next byte */
    unsigned char code = state.memory[state.pc + 1];

    switch (code)
    {
        /* SBC  HL,BC */
        case 0x42: *state.hl = z80_sub_16(*state.hl, *state.bc, state.flags.cy);
                   break;

        /* LD (NN),BC */
        case 0x43: z80_write_mem_16(NN, *state.bc);
                   b = 4;
                   break;

        /* NEG          */
        case 0x44: z80_neg();
                   break;

        /* ADC  HL,BC   */
        case 0x4A: *state.hl = z80_adc_16(*state.hl, *state.bc);
                   break;

        /* LD   BC,(NN) */
        case 0x4B: *state.bc = z80_read_mem_16(NN);
                   b = 4;
                   break;

        /* SBC  HL,DE   */
        case 0x52: *state.hl = z80_sub_16(*state.hl, *state.de, state.flags.cy);
                   break;

        /* LD (NN),DE */
        case 0x53: z80_write_mem_16(NN, *state.de);
                   b = 4;
                   break;

        /* ADC  HL,DE   */
        case 0x5A: *state.hl = z80_adc_16(*state.hl, *state.de);
                   break;

        /* LD   DE,(NN) */
        case 0x5B: *state.de = z80_read_mem_16(NN);
                   b = 4;
                   break;

        /* SBC  HL,HL   */
        case 0x62: *state.hl = z80_sub_16(*state.hl, *state.hl, state.flags.cy);
                   break;

        /* RRD          */
        case 0x67: z80_rrd();
                   break;

        /* ADC  HL,HL */
        case 0x6A: *state.hl = z80_adc_16(*state.hl, *state.hl);
                   break;

        /* RLD          */
        case 0x6F: z80_rld();
                   break;

        /* SBC  HL,SP */
        case 0x72: *state.hl = z80_sub_16(*state.hl, state.sp, state.flags.cy);
                   break;

        /* LD (NN) SP */
        case 0x73: z80_write_mem_16(NN, state.sp);
                   b = 4;
                   break;

        /* ADC  HL,SP */
        case 0x7A: *state.hl = z80_adc_16(*state.hl, state.sp);
                   break;

        /* LD SP (NN) */
        case 0x7B: state.sp = z80_read_mem_16(NN);
                   b = 4;
                   break;

        /* LDI        */
        case 0xA0: z80_ldi();
                   break;

        /* CPI        */
        case 0xA1: z80_cpid(state.memory[*state.hl], 1);
                   break;

        /* LDD        */
        case 0xA8: z80_ldd();
                   break;

        /* CPD        */
        case 0xA9: z80_cpid(state.memory[*state.hl], -1);
                   break;

        /* LDIR       */
        case 0xB0: do z80_ldi(); while (*state.bc != 0);
                   break; 

        /* CPIR       */
        case 0xB1: state.flags.z = 0;

                   /* loop until BC != 0 and z != 1 */
                   while (state.flags.z == 0 && *state.bc != 0)
                       z80_cpid(state.memory[*state.hl], 1);

                   break;

        /* LDDR        */
        case 0xB8: do z80_ldd(); while (*state.bc != 0);
                   break;

        /* CPDR       */
        case 0xB9: state.flags.z = 0;

                   /* loop until BC != 0 and z != 1 */
                   while (state.flags.z == 0 && *state.bc != 0)
                       z80_cpid(state.memory[*state.hl], -1);

                   break;

        default: printf("Unimplemented Z80 ED OP code: %02x\n", code);
    }

    return b;
}


/* Z80 extended OPs - 0xFD branch */
int inline z80_ext_fd_execute()
{
    int b = 2;

    /* displacemente byte */
    uint8_t d;

    /* FDCB family (ROT, BIT, RES, SET)    */
    uint8_t fdcbfam;

    /* FDCB operation     */
    uint8_t fdcbop;

    /* choosen register */
    uint8_t reg;

    /* i already know it's a FD - get the next byte */
    unsigned char code = state.memory[state.pc + 1];

    switch (code)
    {
        /* ADD IY+BC      */
        case 0x09: *state.iy = z80_add_16(*state.iy, *state.bc);
                   break;

        /* ADD IY+DE      */
        case 0x19: *state.iy = z80_add_16(*state.iy, *state.de);
                   break;

        /* LD  IY,NN      */
        case 0x21: *state.iy = NN;
                   b = 4;
                   break;

        /* LD  (NN),IY    */
        case 0x22: z80_write_mem_16(NN, *state.iy);
                   b = 4;
                   break;

        /* INC IY         */
        case 0x23: *state.iy = *state.iy + 1;
                   break;

        /* LD  IYH,N      */
        case 0x26: state.iyh = state.memory[state.pc + 2];
                   b = 3;
                   break;

        /* ADD IY+HL      */
        case 0x29: *state.iy = z80_add_16(*state.iy, *state.iy);
                   break;

        /* LD  IY,(NN)    */
        case 0x2A: *state.iy = z80_read_mem_16(NN);
                   b = 4;
                   break;

        /* DEC IY         */
        case 0x2B: (*state.iy)--; 
                   break;

        /* LD  IYL,N      */
        case 0x2E: state.iyl = state.memory[state.pc + 2];
                   b = 3;
                   break;

        /* INC (IY+d)     */
        case 0x34: d = state.memory[state.pc + 2];
                   state.memory[*state.iy + d] = z80_inr(state.memory[*state.iy + d]);
                   b = 3;
                   break;

        /* DEC (IY+d)     */
        case 0x35: d = state.memory[state.pc + 2];
                   state.memory[*state.iy + d] = z80_dcr(state.memory[*state.iy + d]);
                   b = 3;
                   break;

        /* LD (IY+d),N    */
        case 0x36: d = state.memory[state.pc + 2];
                   state.memory[*state.iy + d] = state.memory[state.pc + 3];
                   b = 4;
                   break;

        /* ADD IY+SP      */
        case 0x39: *state.iy = z80_add_16(*state.iy, state.sp);
                   break;

        /* LD  B,(IY+d)   */
        case 0x46: d = state.memory[state.pc + 2];
                   state.b = state.memory[*state.iy + d];
                   b = 3;
                   break;

        /* LD  C,(IY+d)   */
        case 0x4E: d = state.memory[state.pc + 2];
                   state.c = state.memory[*state.iy + d];
                   b = 3;
                   break;

        /* LD  D,(IY+d)   */
        case 0x56: d = state.memory[state.pc + 2];
                   state.d = state.memory[*state.iy + d];
                   b = 3;
                   break;

        /* LD  E,(IY+d)   */
        case 0x5E: d = state.memory[state.pc + 2];
                   state.e = state.memory[*state.iy + d];
                   b = 3;
                   break;

        /* LD  IYH, C     */
        case 0x60: state.iyh = state.b;
                   break;

        /* LD  IYH, C     */
        case 0x61: state.iyh = state.c;
                   break;

        /* LD  IYH, D     */
        case 0x62: state.iyh = state.d;
                   break;

        /* LD  IYH, E     */
        case 0x63: state.iyh = state.e;
                   break;

        /* LD  IYH, H     */
        case 0x64: state.iyh = state.h;
                   break;

        /* LD  IYH, L     */
        case 0x65: state.iyh = state.l;
                   break;

        /* LD  H,(IY+d)   */
        case 0x66: d = state.memory[state.pc + 2];
                   state.h = state.memory[*state.iy + d];
                   b = 3;
                   break;

        /* LD  IYH, A     */
        case 0x67: state.iyh = state.a;
                   break;

        /* LD  IYL, C     */
        case 0x68: state.iyl = state.b;
                   break;

        /* LD  IYL, C     */
        case 0x69: state.iyl = state.c;
                   break;

        /* LD  IYL, D     */
        case 0x6A: state.iyl = state.d;
                   break;

        /* LD  IYL, E     */
        case 0x6B: state.iyl = state.e;
                   break;

        /* LD  IYL, H     */
        case 0x6C: state.iyl = state.iyh;
                   break;

        /* LD  IYL, IYL   */
        case 0x6D: state.iyl = state.iyl;
                   break;

        /* LD  L,(IY+d)   */
        case 0x6E: d = state.memory[state.pc + 2];
                   state.l = state.memory[*state.iy + d];
                   b = 3;
                   break;

        /* LD  IYL, A     */
        case 0x6F: state.iyl = state.a;
                   break;

        /* MOV (IY+d), B  */
        case 0x70: d = state.memory[state.pc + 2];
                   state.memory[*state.iy + d] = state.b;
                   b = 3;
                   break;

        /* MOV (IY+d), C  */
        case 0x71: d = state.memory[state.pc + 2];
                   state.memory[*state.iy + d] = state.c;
                   b = 3;
                   break;

        /* MOV (IY+d), D  */
        case 0x72: d = state.memory[state.pc + 2];
                   state.memory[*state.iy + d] = state.d;
                   b = 3;
                   break;

        /* MOV (IY+d), E  */
        case 0x73: d = state.memory[state.pc + 2];
                   state.memory[*state.iy + d] = state.e;
                   b = 3;
                   break;

        /* MOV (IY+d), H  */
        case 0x74: d = state.memory[state.pc + 2];
                   state.memory[*state.iy + d] = state.h;
                   b = 3; 
                   break;

        /* MOV (IY+d), L  */
        case 0x75: d = state.memory[state.pc + 2];
                   state.memory[*state.iy + d] = state.l;
                   b = 3;
                   break;

        /* MOV (IY+d), L  */
        case 0x77: d = state.memory[state.pc + 2];
                   state.memory[*state.iy + d] = state.a;
                   b = 3;
                   break;

        /* LD  A,(IY+d)   */
        case 0x7E: d = state.memory[state.pc + 2];
                   state.a = state.memory[*state.iy + d];
                   b = 3;
                   break;

        /* ADD IYH        */
        case 0x84: z80_add(state.iyh);
                   break;

        /* ADD IYL        */
        case 0x85: z80_add(state.iyl);
                   break;

        /* ADD (IY+d)     */
        case 0x86: d = state.memory[state.pc + 2];
                   z80_add(state.memory[*state.iy + d]);
                   b = 3;
                   break;

        /* ADC  IYH      */
        case 0x8C: z80_adc(state.iyh);
                   break;

        /* ADC  IYL      */
        case 0x8D: z80_adc(state.iyl);
                   break;

        /* ADC (IY+d)    */
        case 0x8E: d = state.memory[state.pc + 2];
                   z80_adc(state.memory[*state.iy + d]);
                   b = 3;
                   break;

        /* SUB IYH        */
        case 0x94: z80_sub(state.iyh);
                   break;

        /* SUB IYH        */
        case 0x95: z80_sub(state.iyl);
                   break;

        /* SUB (IY+d)     */
        case 0x96: d = state.memory[state.pc + 2];
                   z80_sub(state.memory[*state.iy + d]);
                   b = 3;
                   break;

        /* SBC  IYH       */
        case 0x9C: z80_sbc(state.iyh);
                   break;

        /* SBC  IYL       */
        case 0x9D: z80_sbc(state.iyl);
                   break;

        /* SBC (IY+d)     */
        case 0x9E: d = state.memory[state.pc + 2];
                   z80_sbc(state.memory[*state.iy + d]);
                   b = 3;
                   break;

        /* ANA  IYH       */
        case 0xA4: z80_ana(state.iyh);
                   break;

        /* ANA  IYL       */
        case 0xA5: z80_ana(state.iyl);
                   break;

        /* ANA (IY+d)     */
        case 0xA6: d = state.memory[state.pc + 2];
                   z80_ana(state.memory[*state.iy + d]);
                   b = 3;
                   break;

        /* XRA  IYH       */
        case 0xAC: z80_xra(state.iyh);
                   break;

        /* XRA  IYL       */
        case 0xAD: z80_xra(state.iyl);
                   break;

        /* XRA (IY+d)     */
        case 0xAE: d = state.memory[state.pc + 2];
                   z80_xra(state.memory[*state.iy + d]);
                   b = 3;
                   break;

        /* OR    IYH      */
        case 0xB4: z80_ora(state.iyh);
                   break;

        /* OR    IYL      */
        case 0xB5: z80_ora(state.iyl); 
                   break;

        /* ORA (IY+d)     */
        case 0xB6: d = state.memory[state.pc + 2];
                   z80_ora(state.memory[*state.iy + d]);
                   b = 3;
                   break;

        /* CMP   IYH      */
        case 0xBC: z80_cmp(state.iyh); 
                   break;

        /* CMP   IYL      */
        case 0xBD: z80_cmp(state.iyl);
                   break;

        /* CMP (IY+d)     */
        case 0xBE: d = state.memory[state.pc + 2];
                   z80_cmp(state.memory[*state.iy + d]);
                   b = 3;
                   break;

        /* FDCB Operation */
        case 0xCB: d = state.memory[state.pc + 2];
                   fdcbfam = state.memory[state.pc + 3] >> 6;
                   reg = state.memory[state.pc + 3] & 0x07;

                   switch (fdcbfam)
                   {
                       /* Rotate Family */
                       case 0x00:
                           fdcbop = state.memory[state.pc + 3] & 0x38;

                           switch(fdcbop)
                           {
                               /* RLC (IY+d) REG */
                               case 0x00: *regs_dst[reg] = 
                                          z80_rl(&state.memory[*state.iy + d], 1);
                                          break;

                               /* RRC (IY+d) REG */
                               case 0x08: *regs_dst[reg] = 
                                          z80_rr(&state.memory[*state.iy + d], 1);
                                          break;

                               /* RL  (IY+d) REG */
                               case 0x10: *regs_dst[reg] = 
                                          z80_rl(&state.memory[*state.iy + d], 0);
                                          break;

                               /* RR  (IY+d) REG */
                               case 0x18: *regs_dst[reg] = 
                                          z80_rr(&state.memory[*state.iy + d], 0);
                                          break;

                               /* SLA (IY+d) REG */
                               case 0x20: *regs_dst[reg] = 
                                          z80_sl(&state.memory[*state.iy + d], 0);
                                          break;

                               /* SRA (IY+d) REG */
                               case 0x28: *regs_dst[reg] = 
                                          z80_sr(&state.memory[*state.iy + d], 1);
                                          break;

                               /* SLL (IY+d) REG */
                               case 0x30: *regs_dst[reg] = 
                                          z80_sl(&state.memory[*state.iy + d], 1);
                                          break;

                               /* SRL (IY+d) REG */
                               case 0x38: *regs_dst[reg] = 
                                          z80_sr(&state.memory[*state.iy + d], 0);
                                          break;

/*                             default: printf("Unimplemented FDCB family: %02x - MEM: %02x %02x\n",
                                               fdcbfam,
                                               state.memory[state.pc + 2],
                                               state.memory[state.pc + 3]); */
                           }
                           break;

                       /* BIT Family */
                       case 0x01: z80_bit(&state.memory[*state.iy + d],
                                      (state.memory[state.pc + 3] >> 3) & 0x07, d);
                                  break;

                       /* RES Family */
                       case 0x02: *regs_dst[reg] = z80_res(&state.memory[*state.iy + d],
                                                       (state.memory[state.pc + 3] >> 3) & 0x07);
                                  break;

                       /* SET Family */
                       case 0x03: *regs_dst[reg] = z80_set(&state.memory[*state.iy + d],
                                                       (state.memory[state.pc + 3] >> 3) & 0x07);
                                  break;

/*                     default: printf("Unimplemented FDCB family: %02x - MEM: %02x %02x\n",
                                       fdcbfam,
                                       state.memory[state.pc + 2],
                                       state.memory[state.pc + 3]); */

                   }

                   b = 4;
                   break;

        /* POP   IY       */       
        case 0xE1: *state.iy = z80_read_mem_16(state.sp); 
                   state.sp += 2;
                   break;

        /* PUSH  IY       */
        case 0xE5: z80_write_mem_16(state.sp - 2, *state.iy);
                   state.sp -= 2;
                   break;

        default: // printf("Unimplemented Z80 FD OP code: %02x\n", code);
                 state.skip_cycle = 1;
                 b = 1; 
    }

    return b;
}

/* really execute the OP. Could be ran by normal execution or *
 * because an interrupt occours                               */
int inline z80_execute(unsigned char code)
{
    int b = 1;
    uint8_t  xchg;
    uint8_t  *p;

    switch (code)
    {
        /* NOP       */
        case 0x00: break;                           

        /* LXI  B    */
        case 0x01: *state.bc = ADDR;   
                   b = 3;
                   break;

        /* STAX B    */
        case 0x02: state.memory[*state.bc] = state.a;                    
                   break;                          

        /* INX  B    */
        case 0x03: (*state.bc)++;                    
                   break;        

        /* INR  B    */
        case 0x04: state.b = z80_inr(state.b);                   
                   break;   

        /* DCR  B    */
        case 0x05: state.b = z80_dcr(state.b);                     
                   break;

        /* MVI  B    */
        case 0x06: state.b = state.memory[state.pc + 1];  
                   b = 2;
                   break;

        /* RLCA      */
        case 0x07: z80_rla(&state.a, 1);
                   break;

        /* NOP       */
        case 0x08: break;                          

        /* DAD  B    */
        case 0x09: *state.hl = dad_16(*state.hl, *state.bc);    
                   break;

        /* LDAX B    */
        case 0x0A: state.a = state.memory[*state.bc];          
                   break;

        /* DCX  B    */
        case 0x0B: (*state.bc)--;
                   break;

        /* INR  C    */
        case 0x0C: state.c = z80_inr(state.c);                 
                   break;   

        /* DCR  C    */
        case 0x0D: state.c = z80_dcr(state.c);            
                   break;   

        /* MVI  C    */
        case 0x0E: state.c = state.memory[state.pc + 1]; 
                   b = 2;
                   break;

        /* RRC       */
        case 0x0F: z80_rra(&state.a, 1);       
                   break;

        /* NOP       */
        case 0x10: break;                           

        /* LXI  D    */
        case 0x11: *state.de = ADDR;   
                   b = 3;
                   break;

        /* STAX D    */
        case 0x12: state.memory[*state.de] = state.a;            
                   break;

        /* INX  D    */
        case 0x13: (*state.de)++;
                   break;

        /* INR  D    */
        case 0x14: state.d = z80_inr(state.d);               
                   break;   

        /* DCR  D    */
        case 0x15: state.d = z80_dcr(state.d);              
                   break;   

        /* MVI  D    */
        case 0x16: state.d = state.memory[state.pc + 1];  
                   b = 2;
                   break;

        /* RLA       */
        case 0x17: z80_rla(&state.a, 0);
                   break;

        /* NOP       */
        case 0x18: break;                    


        /* DAD  D    */
        case 0x19: *state.hl = dad_16(*state.hl, *state.de);
                   break;

        /* LDAX D    */
        case 0x1A: state.a = state.memory[*state.de];            
                   break;

        /* DCX  D    */
        case 0x1B: (*state.de)--;
                   break;

        /* INR  E    */
        case 0x1C: state.e = z80_inr(state.e);                  
                   break;

        /* DCR  E    */
        case 0x1D: state.e = z80_dcr(state.e);                       
                   break;

        /* MVI  E    */
        case 0x1E: state.e = state.memory[state.pc + 1];   
                   b = 2;
                   break;

        /* RRA       */
        case 0x1F: z80_rra(&state.a, 0);
                   break;

        /* RIM       */
        case 0x20: break;                        

        /* LXI  H    */
        case 0x21: *state.hl = ADDR; 
                   b = 3;
                   break;

        /* SHLD      */
        case 0x22: z80_write_mem_16(ADDR, *state.hl); 
                   b = 3;
                   break;

        /* INX  H    */
        case 0x23: (*state.hl)++;
                   break;

        /* INR  H    */
        case 0x24: state.h = z80_inr(state.h);                      
                   break;

        /* DCR  H    */
        case 0x25: state.h = z80_dcr(state.h);                       
                   break;

        /* MVI  H    */
        case 0x26: state.h = state.memory[state.pc + 1];   
                   b = 2;
                   break;

        /* DAA       */
        case 0x27: z80_daa();
                   break;                            

        /* NOP       */
        case 0x28: break;                           

        /* DAD  H    */
        case 0x29: *state.hl = dad_16(*state.hl, *state.hl);
                   break;

        /* LHLD      */
        case 0x2A: *state.hl = z80_read_mem_16(ADDR);    
                   b = 3;
                   break;

        /* DCX  H    */
        case 0x2B: (*state.hl)--;
                   break;

        /* INR  L    */
        case 0x2C: state.l = z80_inr(state.l);                       
                   break;

        /* DCR  L    */
        case 0x2D: state.l = z80_dcr(state.l);                      
                   break;

        /* MVI  L    */
        case 0x2E: state.l = state.memory[state.pc + 1];  
                   b = 2;
                   break;

        /* CMA  A    */
        case 0x2F: state.a = ~state.a;             
           
                   state.flags.ac = 1; 
                   state.flags.n  = 1; 
                   state.flags.u3 = ((state.a & 0x08) != 0);
                   state.flags.u5 = ((state.a & 0x20) != 0);
  
                   break;

        /* SIM       */
        case 0x30: break;                     
 
        /* LXI  SP   */
        case 0x31: state.sp = ADDR;
                   b = 3;
                   break;

        /* STA       */
        case 0x32: state.memory[ADDR] = state.a;          
                   b = 3;
                   break;

        /* INX  SP   */
        case 0x33: state.sp++;           
                   break;

        /* INR  M    */
        case 0x34: state.memory[*state.hl] = z80_inr(state.memory[*state.hl]);
                   break;

        /* DCR  M    */
        case 0x35: state.memory[*state.hl] = z80_dcr(state.memory[*state.hl]);
                   break;

        /* MVI  M    */
        case 0x36: state.memory[*state.hl] = state.memory[state.pc + 1];
                   b = 2;
                   break;

        /* STC       */
        case 0x37: state.flags.cy = 1;              

                   state.flags.ac = 0;
                   state.flags.n  = 0;
                   state.flags.u3 = ((state.a & 0x08) != 0);
                   state.flags.u5 = ((state.a & 0x20) != 0);

                   break;

        /* NOP       */
        case 0x38: break;                          

        /* DAD  SP   */
        case 0x39: *state.hl = dad_16(*state.hl, state.sp);
                   break;

        /* LDA       */
        case 0x3A: state.a = state.memory[ADDR];        
                   b = 3;
                   break;

        /* DCX  SP   */
        case 0x3B: state.sp--;                    
                   break;

        /* INR  A    */
        case 0x3C: state.a = z80_inr(state.a);                      
                   break;

        /* DCR  A    */
        case 0x3D: state.a = z80_dcr(state.a);                
                   break;

        /* MVI  A   */
        case 0x3E: state.a = state.memory[state.pc + 1];   
                   b = 2;
                   break;

        /* CCF      */
        case 0x3F: state.flags.ac = state.flags.cy;
                   state.flags.cy = !state.flags.cy;
                   state.flags.n  = 0;

                   state.flags.u3 = ((state.a & 0x08) != 0);
                   state.flags.u5 = ((state.a & 0x20) != 0);

                   break;

        /* MOV  B,B  */
        case 0x40: state.b = state.b; 
                   break;  

        /* MOV  B,C  */
        case 0x41: state.b = state.c; 
                   break;  

        /* MOV  B,D  */
        case 0x42: state.b = state.d; 
                   break;  

        /* MOV  B,E  */
        case 0x43: state.b = state.e; 
                   break;  

        /* MOV  B,H  */
        case 0x44: state.b = state.h; 
                   break;  

        /* MOV  B,L  */
        case 0x45: state.b = state.l; 
                   break;  

        /* MOV  B,M  */
        case 0x46: state.b = state.memory[*state.hl]; 
                   break;  

        /* MOV  B,A  */
        case 0x47: state.b = state.a; 
                   break;  

        /* MOV  C,B  */
        case 0x48: state.c = state.b; 
                   break;  

        /* MOV  C,C  */
        case 0x49: state.c = state.c; 
                   break;  

        /* MOV  C,D  */
        case 0x4A: state.c = state.d; 
                   break;  

        /* MOV  C,E  */
        case 0x4B: state.c = state.e; 
                   break;  

        /* MOV  C,H  */
        case 0x4C: state.c = state.h; 
                   break;  

        /* MOV  C,L  */
        case 0x4D: state.c = state.l; 
                   break;  

        /* MOV  C,M  */
        case 0x4E: state.c = state.memory[*state.hl]; 
                   break;  

        /* MOV  C,A  */
        case 0x4F: state.c = state.a; 
                   break;  

        /* MOV  D,B  */
        case 0x50: state.d = state.b; 
                   break;  

        /* MOV  D,C  */
        case 0x51: state.d = state.c; 
                   break;  

        /* MOV  D,D  */
        case 0x52: state.d = state.d; 
                   break;  

        /* MOV  D,E  */
        case 0x53: state.d = state.e; 
                   break;  

        /* MOV  D,H  */
        case 0x54: state.d = state.h; 
                   break;  

        /* MOV  D,L  */
        case 0x55: state.d = state.l; 
                   break;  

        /* MOV  D,M  */
        case 0x56: state.d = state.memory[*state.hl]; 
                   break;  

        /* MOV  D,A  */
        case 0x57: state.d = state.a; 
                   break;  

        /* MOV  E,B  */
        case 0x58: state.e = state.b; 
                   break;  

        /* MOV  E,C  */
        case 0x59: state.e = state.c; 
                   break;  

        /* MOV  E,D  */
        case 0x5A: state.e = state.d; 
                   break;  

        /* MOV  E,E  */
        case 0x5B: state.e = state.e; 
                   break;  

        /* MOV  E,H  */
        case 0x5C: state.e = state.h; 
                   break;  

        /* MOV  E,L  */
        case 0x5D: state.e = state.l; 
                   break;  

        /* MOV  E,M  */
        case 0x5E: state.e = state.memory[*state.hl]; 
                   break;  

        /* MOV  E,A  */
        case 0x5F: state.e = state.a; 
                   break;  

        /* MOV  H,B  */
        case 0x60: state.h = state.b; 
                   break;  

        /* MOV  H,C  */
        case 0x61: state.h = state.c; 
                   break;  

        /* MOV  H,D  */
        case 0x62: state.h = state.d; 
                   break;  

        /* MOV  H,E  */
        case 0x63: state.h = state.e; 
                   break;  

        /* MOV  H,H  */
        case 0x64: state.h = state.h; 
                   break;  

        /* MOV  H,L  */
        case 0x65: state.h = state.l; 
                   break;  

        /* MOV  H,M  */
        case 0x66: state.h = state.memory[*state.hl]; 
                   break;  

        /* MOV  H,A  */
        case 0x67: state.h = state.a; 
                   break;  

        /* MOV  L,B  */
        case 0x68: state.l = state.b; 
                   break;  

        /* MOV  L,C  */
        case 0x69: state.l = state.c; 
                   break;  

        /* MOV  L,D  */
        case 0x6A: state.l = state.d; 
                   break;  

        /* MOV  L,E  */
        case 0x6B: state.l = state.e; 
                   break;  

        /* MOV  L,H  */
        case 0x6C: state.l = state.h; 
                   break;  

        /* MOV  L,L  */
        case 0x6D: state.l = state.l; 
                   break;  

        /* MOV  L,M  */
        case 0x6E: state.l = state.memory[*state.hl]; 
                   break;  

        /* MOV  L,A  */
        case 0x6F: state.l = state.a; 
                   break;  

        /* MOV  M,B  */
        case 0x70: state.memory[*state.hl] = state.b;
                   break;

        /* MOV  M,C  */
        case 0x71: state.memory[*state.hl] = state.c;
                   break;

        /* MOV  M,D  */
        case 0x72: state.memory[*state.hl] = state.d;
                   break;

        /* MOV  M,E  */
        case 0x73: state.memory[*state.hl] = state.e;
                   break;

        /* MOV  M,H  */
        case 0x74: state.memory[*state.hl] = state.h;
                   break;

        /* MOV  M,L  */
        case 0x75: state.memory[*state.hl] = state.l;
                   break;

        /* HLT       */
        case 0x76: return 1;

        /* MOV  M,A  */
        case 0x77: state.memory[*state.hl] = state.a;
                   break;

        /* MOV  A,B  */
        case 0x78: state.a = state.b; 
                   break;  

        /* MOV  A,C  */
        case 0x79: state.a = state.c; 
                   break;  

        /* MOV  A,D  */
        case 0x7A: state.a = state.d; 
                   break;  

        /* MOV  A,E  */
        case 0x7B: state.a = state.e; 
                   break;  

        /* MOV  A,H  */
        case 0x7C: state.a = state.h; 
                   break;  

        /* MOV  A,L  */
        case 0x7D: state.a = state.l; 
                   break;  

        /* MOV  A,M  */
        case 0x7E: state.a = state.memory[*state.hl]; 
                   break;  

        /* MOV  A,A  */
        case 0x7F: state.a = state.a; 
                   break;  

        /* ADD  B    */
        case 0x80: z80_add(state.b);
                   break;   

        /* ADD  C    */
        case 0x81: z80_add(state.c);
                   break;   

        /* ADD  D    */
        case 0x82: z80_add(state.d);
                   break;   

        /* ADD  E    */
        case 0x83: z80_add(state.e);
                   break;   

        /* ADD  H    */
        case 0x84: z80_add(state.h);
                   break;   

        /* ADD  L    */
        case 0x85: z80_add(state.l);
                   break;   

        /* ADD  M    */
        case 0x86: z80_add(state.memory[*state.hl]); 
                   break;   

        /* ADD  A    */
        case 0x87: z80_add(state.a);
                   break;   

        /* ADC  B    */
        case 0x88: z80_adc(state.b); 
                   break;   

        /* ADC  C    */
        case 0x89: z80_adc(state.c);
                   break;   

        /* ADC  D    */
        case 0x8A: z80_adc(state.d);
                   break;   

        /* ADC  E    */
        case 0x8B: z80_adc(state.e);
                   break;   

        /* ADC  H    */
        case 0x8C: z80_adc(state.h); 
                   break;   

        /* ADC  L    */
        case 0x8D: z80_adc(state.l);
                   break;   

        /* ADC  M    */
        case 0x8E: z80_adc(state.memory[*state.hl]);
                   break;   

        /* ADC  A    */
        case 0x8F: z80_adc(state.a);
                   break;   

        /* SUB  B    */
        case 0x90: z80_sub(state.b);
                   break;   

        /* SUB  C    */
        case 0x91: z80_sub(state.c);
                   break;   

        /* SUB  D    */
        case 0x92: z80_sub(state.d);
                   break;   

        /* SUB  E    */
        case 0x93: z80_sub(state.e);
                   break;   

        /* SUB  H    */
        case 0x94: z80_sub(state.h);
                   break;   

        /* SUB  L    */
        case 0x95: z80_sub(state.l);
                   break;   

        /* SUB  M    */
        case 0x96: z80_sub(state.memory[*state.hl]);
                   break;   

        /* SUB  A    */
        case 0x97: z80_sub(state.a);
                   break;   

        /* SBC  B    */
        case 0x98: z80_sbc(state.b);
                   break;   

        /* SBC  C    */
        case 0x99: z80_sbc(state.c);
                   break;   

        /* SBC  D    */
        case 0x9a: z80_sbc(state.d);
                   break;   

        /* SBC  E    */
        case 0x9b: z80_sbc(state.e);
                   break;   

        /* SBC  H    */
        case 0x9c: z80_sbc(state.h);
                   break;   

        /* SBC  L    */
        case 0x9d: z80_sbc(state.l);
                   break;   

        /* SBC  M    */
        case 0x9E: z80_sbc(state.memory[*state.hl]); 
                   break;   

        /* SBC  A    */
        case 0x9f: z80_sbc(state.a); 
                   break;   

        /* ANA  B    */
        case 0xA0: z80_ana(state.b);
                   break;

        /* ANA  C    */
        case 0xA1: z80_ana(state.c);
                   break;

        /* ANA  D    */ 
        case 0xA2: z80_ana(state.d);
                   break;

        /* ANA  E    */
        case 0xA3: z80_ana(state.e);
                   break;

        /* ANA  H    */
        case 0xA4: z80_ana(state.h);
                   break;

        /* ANA  L    */
        case 0xA5: z80_ana(state.l);
                   break;

        /* ANA  M    */
        case 0xA6: z80_ana(state.memory[*state.hl]);
                   break;

        /* ANA  A    */
        case 0xA7: z80_ana(state.a);
                   break;

        /* XRA  B    */
        case 0xA8: z80_xra(state.b);
                   break;

        /* XRA  C    */
        case 0xA9: z80_xra(state.c);
                   break;

        /* XRA  D    */
        case 0xAA: z80_xra(state.d);
                   break;

        /* XRA  E    */
        case 0xAB: z80_xra(state.e);
                   break;

        /* XRA  H    */
        case 0xAC: z80_xra(state.h);
                   break;

        /* XRA  L    */
        case 0xAD: z80_xra(state.l);
                   break;

        /* XRA  M    */
        case 0xAE: z80_xra(state.memory[*state.hl]);
                   break;

        /* XRA  A    */
        case 0xAF: z80_xra(state.a);
                   break;

        /* ORA  B    */
        case 0xB0: z80_ora(state.b);
                   break;

        /* ORA  C    */
        case 0xB1: z80_ora(state.c);
                   break;

        /* ORA  D    */ 
        case 0xB2: z80_ora(state.d);
                   break;

        /* ORA  E    */
        case 0xB3: z80_ora(state.e);
                   break;

        /* ORA  H    */
        case 0xB4: z80_ora(state.h);
                   break;

        /* ORA  L    */
        case 0xB5: z80_ora(state.l);
                   break;

        /* ORA  M    */
        case 0xB6: z80_ora(state.memory[*state.hl]);
                   break;

        /* ORA  A    */
        case 0xB7: z80_ora(state.a);
                   break;

        /* CMP  B    */
        case 0xB8: z80_cmp(state.b);
                   break;

        /* CMP  C    */
        case 0xB9: z80_cmp(state.c);
                   break;

        /* CMP  D    */
        case 0xBA: z80_cmp(state.d);
                   break;

        /* CMP  E    */
        case 0xBB: z80_cmp(state.e);
                   break;

        /* CMP  H    */
        case 0xBC: z80_cmp(state.h);
                   break;

        /* CMP  L    */
        case 0xBD: z80_cmp(state.l);
                   break;

        /* CMP  M    */
        case 0xBE: z80_cmp(state.memory[*state.hl]);
                   break;

        /* CMP  A    */
        case 0xBF: z80_cmp(state.a);
                   break;


        /* RNZ       */
        case 0xC0: if (state.flags.z == 0)
                       return z80_ret();
                   break;

        /* POP  B    */
        case 0xC1: *state.bc = z80_read_mem_16(state.sp); 
                   state.sp += 2;
                   break;

        /* JNZ  addr */
        case 0xC2: if (state.flags.z == 0)
                   {
                       state.pc = ADDR;
                       return 0;
                   } 

                   b = 3;                      
                   break;

        /* JMP  addr */
        case 0xC3: state.pc = ADDR;                
                   return 0;

        /* CNZ        */
        case 0xC4: if (state.flags.z == 0)
                       return z80_call(ADDR);

                   b = 3;
                   break;

        /* PUSH B    */
        case 0xC5: z80_write_mem_16(state.sp - 2, *state.bc);
                   state.sp -= 2;
                   break;

        /* ADI       */
        case 0xC6: z80_add(state.memory[state.pc + 1]);
                   b = 2;
                   break;

        /* RST  0    */
        case 0xC7: return z80_intr(0x0008 * 0);
                  
        /* RZ        */
        case 0xC8: if (state.flags.z)
                       return z80_ret();
                   break;

        /* RET       */
        case 0xC9: return z80_ret();

        /* JZ        */
        case 0xCA: if (state.flags.z)
                   {
                       state.pc = ADDR;
                       return 0;
                   }

                   b = 3;
                   break;
       
        /* CB        */
        case 0xCB: b = z80_ext_cb_execute();
                   break;
 
        /* CZ        */
        case 0xCC: if (state.flags.z)
                       return z80_call(ADDR);

                   b = 3;
                   break;
 
        /* CALL addr */
        case 0xCD: return z80_call(ADDR);

        /* ACI       */
        case 0xCE: z80_adc(state.memory[state.pc + 1]);
                   b = 2;
                   break;

        /* RST  1    */
        case 0xCF: return z80_intr(0x0008 * 1);
                  
        /* RNC       */
        case 0xD0: if (state.flags.cy == 0)
                       return z80_ret();
                   break;

        /* POP  D    */
        case 0xD1: *state.de = z80_read_mem_16(state.sp); 
                   state.sp += 2;
                   break;

        /* JNC       */
        case 0xD2: if (state.flags.cy == 0)
                   {
                       state.pc = ADDR;
                       return 0;
                   }

                   b = 3;
                   break;

        /* OUT       */
        case 0xD3: b = 2;
                   break;

        /* CNC        */
        case 0xD4: if (state.flags.cy == 0)
                       return z80_call(ADDR);

                   b = 3;
                   break;

        /* PUSH D    */
        case 0xD5: z80_write_mem_16(state.sp - 2, *state.de);
                   state.sp -= 2;
                   break;

        /* SUI       */
        case 0xD6: z80_sub(state.memory[state.pc + 1]);
                   b = 2;
                   break;

        /* RST  2    */
        case 0xD7: return z80_intr(0x0008 * 2);

        /* RC        */
        case 0xD8: if (state.flags.cy)
                       return z80_ret();
                   break;

        /* NOP       */
        case 0xD9: break;

        /* JC        */
        case 0xDA: if (state.flags.cy)
                   {
                       state.pc = ADDR;
                       return 0;
                   }

                   b = 3;
                   break;

        /* IN        */
        case 0xDB: b = 2;    /* every rom must override this behaviour */
                   break;

        /* CC        */
        case 0xDC: if (state.flags.cy)
                       return z80_call(ADDR);

                   b = 3;
                   break;

        /* Z80 - DD  */
        case 0xDD: b = z80_ext_dd_execute();
                   break;

        /* SBI       */
        case 0xDE: z80_sbc(state.memory[state.pc + 1]);
                   b = 2;
                   break;

        /* RPO       */
        case 0xE0: if (state.flags.p == 0)
                       return z80_ret();
                   break;

        /* POP  H    */
        case 0xE1: *state.hl = z80_read_mem_16(state.sp); 
                   state.sp += 2;
                   break;
    
        /* JPO       */
        case 0xE2: if (state.flags.p == 0)
                   {
                       state.pc = ADDR;
                       return 0;
                   }

                   b = 3;
                   break;

        /* XTHL      */
        case 0xE3: xchg = state.memory[state.sp]; 
                   state.memory[state.sp] = state.l;
                   state.l = xchg;
                   xchg = state.memory[state.sp + 1];
                   state.memory[state.sp + 1] = state.h;
                   state.h = xchg;
                   break;     

        /* CPO       */
        case 0xE4: if (state.flags.p == 0)
                       return z80_call(ADDR);

                   b = 3;
                   break;

        /* PUSH H    */
        case 0xE5: z80_write_mem_16(state.sp - 2, *state.hl);
                   state.sp -= 2;
                   break;

        /* ANI       */
        case 0xE6: z80_ana(state.memory[state.pc + 1]);
                   b = 2;                      
                   break;

        /* RPE       */
        case 0xE8: if (state.flags.p)
                       return z80_ret();
                   break;

        /* PCHL      */
        case 0xE9: state.pc = *state.hl; 
                   return 0;

        /* JPE       */
        case 0xEA: if (state.flags.p)
                   {
                       state.pc = ADDR;
                       return 0;
                   }

                   b = 3;
                   break;

        /* XCHG      */
        case 0xEB: xchg = state.h;
                   state.h = state.d;
                   state.d = xchg;
                   xchg = state.l;
                   state.l = state.e;
                   state.e = xchg; 
                   break;      

        /* CPE       */
        case 0xEC: if (state.flags.p)
                       return z80_call(ADDR);

                   b = 3;
                   break;

        /* Z80 - ED  */
        case 0xED: b = z80_ext_ed_execute(); 
                   break;

        /* XRI       */
        case 0xEE: z80_xra(state.memory[state.pc + 1]);
                   b = 2;
                   break;

        /* RST  5    */
        case 0xEF: return z80_intr(0x0008 * 5);
                  
        /* RP        */
        case 0xF0: if (state.flags.s == 0)
                       return z80_ret();
                   break;

        /* POP  PSW  */
        case 0xF1: p = (uint8_t *) &state.flags;
                   *p        = state.memory[state.sp];
                   state.a   = state.memory[state.sp + 1];

                   state.sp += 2;
                   break;  

        /* JP        */
        case 0xF2: if (state.flags.s == 0)
                   {
                       state.pc = ADDR;
                       return 0;
                   }

                   b = 3;
                   break;
      
        /* DI        */
        case 0xF3: state.int_enable = 0;
                   break;
 
        /* CP        */
        case 0xF4: if (state.flags.p)
                       return z80_call(ADDR);

                   b = 3;
                   break;
 
        /* PUSH PSW  */
        case 0xF5: p = (uint8_t *) &state.flags;
                   state.memory[state.sp - 1] = state.a;
                   state.memory[state.sp - 2] = *p;
                   state.sp -= 2;
                   break;

        /* ORI       */
        case 0xF6: z80_ora(state.memory[state.pc + 1]);
                   b = 2;
                   break;

        /* RST  6    */
        case 0xF7: return z80_intr(0x0008 * 6);
                  
        /* RM        */
        case 0xF8: if (state.flags.s)
                       return z80_ret();
                   break;

        /* SPHL     */
        case 0xF9: state.sp = *state.hl;
                   break;

        /* JM        */
        case 0xFA: if (state.flags.s)
                   {
                       state.pc = ADDR;
                       return 0;
                   }

                   b = 3;
                   break;

        /* EI       */
        case 0xFB: state.int_enable = 1; 
                   break;

        /* CM        */
        case 0xFC: if (state.flags.s)
                       return z80_call(ADDR);

                   b = 3;
                   break;

        /* Z80 - FD  */
        case 0xFD: b = z80_ext_fd_execute();
                   break;

        /* CPI      */
        case 0xFE: z80_cmp(state.memory[state.pc + 1]);
                   b = 2;                      
                   break;

        /* RST  7    */
        case 0xFF: return z80_intr(0x0008 * 7);
                  
        default:
            printf("UNKNOWN OP CODE: %02x\n", code);
            return 1;
    }

    /* make the PC points to the next instruction */
    state.pc += b;

    return 0;
}


/* init registers, flags and state.memory of intel 8080 system */
z80_state_t *z80_init()
{
    /* init as z80? */ 
    z80 = 1;

    /* wipe all the structs */
    bzero(&state, sizeof(z80_state_t));

/* 16 bit values just point to the first reg of the pairs */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        state.hl = (uint16_t *) &state.l;
        state.bc = (uint16_t *) &state.c;
        state.de = (uint16_t *) &state.e;
        state.ix = (uint16_t *) &state.ixl;
        state.iy = (uint16_t *) &state.iyl;
#else
        state.hl = (uint16_t *) &state.h;
        state.bc = (uint16_t *) &state.b;
        state.de = (uint16_t *) &state.d;
        state.ix = (uint16_t *) &state.ixh;
        state.iy = (uint16_t *) &state.iyh;
#endif


    if (z80)
    {
        /* set sp and A to ffff (only Z80?) */
        state.sp = 0xffff;
        state.a  = 0xff;
        state.ixl = 0x0000;
        state.ixh = 0x0000;
        state.iyl = 0x0000;
        state.iyh = 0x0000;

        state.b  = 0x7f;
        state.c  = 0xbc;
        state.d  = 0x00;
        state.e  = 0x00;
        state.h  = 0x34;
        state.l  = 0xc0;

        regs_dst = malloc(8 * sizeof(uint8_t *));

        regs_dst[0x00] = &state.b;
        regs_dst[0x01] = &state.c;
        regs_dst[0x02] = &state.d;
        regs_dst[0x03] = &state.e;
        regs_dst[0x04] = &state.h;
        regs_dst[0x05] = &state.l;
        regs_dst[0x06] = &dummy;
        regs_dst[0x07] = &state.a;

        regs_src = malloc(8 * sizeof(uint8_t *));

        regs_src[0x00] = &state.b;
        regs_src[0x01] = &state.c;
        regs_src[0x02] = &state.d;
        regs_src[0x03] = &state.e;
        regs_src[0x04] = &state.h;
        regs_src[0x05] = &state.l;
        regs_src[0x06] = &state.memory[*state.hl];
        regs_src[0x07] = &state.a;
    }
 
    /* just set 1 to unused 1 flag */
    if (z80)
    {
        state.flags.cy = 1;
        state.flags.n  = 1;
        state.flags.p  = 1;
        state.flags.u3 = 1;
        state.flags.ac = 1;
        state.flags.u5 = 1;
        state.flags.z  = 1;
        state.flags.s  = 1;
    }
    else
    {
        state.flags.n  = 1;
        state.flags.u3 = 0;
        state.flags.u5 = 0;
    }

    /* flags shortcut */
    state.f = (uint8_t *) &state.flags;
 
    /* setup parity array */
    z80_calc_parity_array();

    /* sz5p3 mask array   */
    z80_calc_sz5p3_array();

    /* sz5p3c mask array  */
    z80_calc_sz5p3c_array();

    /* szc_16 mask array  */
    z80_calc_szc_16_array();

    return &state;
}