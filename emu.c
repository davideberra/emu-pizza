/*

   Space Invaders, (C) Taito 1978, Midway 1979

   CPU: Intel 8080 @ 2MHz (CPU similar to the (newer) Zilog Z80)    

   Interrupts: $cf (RST 8) at the start of vblank, 
               $d7 (RST $10) at the end of vblank.    

   Video: 256(x)*224(y) @ 60Hz, vertical monitor. Colours are simulated with a    
   plastic transparent overlay and a background picture.    
   Video hardware is very simple: 7168 bytes 1bpp bitmap (32 bytes per scan).    

   Sound: SN76477 and samples.    

   Memory map:    
    ROM    
    $0000-$07ff:    invaders.h    
    $0800-$0fff:    invaders.g    
    $1000-$17ff:    invaders.f    
    $1800-$1fff:    invaders.e    

    RAM    
    $2000-$23ff:    work RAM    
    $2400-$3fff:    video RAM    

    $4000-:     RAM mirror   

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>

/* struct to simulate 8080 registers and flags */
typedef struct flags_s
{
    uint8_t  z:1;
    uint8_t  s:1;
    uint8_t  p:1;
    uint8_t  cy:1;
    uint8_t  ac:1;
    uint8_t  pad:3;
} flags_t;

typedef struct state_s
{
    uint8_t  a;
    uint8_t  b;
    uint8_t  c;
    uint8_t  d;
    uint8_t  e;
    uint8_t  h;
    uint8_t  l;
    uint16_t sp;
    uint16_t pc;
    flags_t  flags;
    uint8_t  int_enable;
} state_t;

/* super mega global describing 8080 register and flags state */
state_t state;

/* all 16 bits of memory reachable by 8080 */
unsigned char   memory[65536];

/* precomputed parity check array */
unsigned char   parity[256];

/* macros to access pair of registers */
#define BC (uint16_t) (((uint16_t) state.b << 8) + state.c)
#define DE (uint16_t) (((uint16_t) state.d << 8) + state.e)
#define HL (uint16_t) (((uint16_t) state.h << 8) + state.l)

/* macro to access addresses passed as parameters */
#define ADDR (uint16_t) memory[state.pc + 1] + \
             (uint16_t) (memory[state.pc + 2] << 8)

/* print out the register and flags state */
void print_state()
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
    printf("Z:  %x   S:  %x\n", state.flags.z, state.flags.s);
    printf("P:  %x   CY: %x   AC: %x\n", state.flags.p, state.flags.cy,
                                         state.flags.ac);
}

/* print out th video RAM */
void print_video_ram()
{
    int i;

    for (i=0; i<8192; i++)
        if (memory[0x2400 + i])
            printf("PIXEL LINEA %d COLONNA %d\n", i / 224, i % 224);
}

/* calc flags except for AC */
void set_flags(uint16_t answer)
{
    state.flags.z  = ((answer & 0xff) == 0);    
    state.flags.s  = ((answer & 0x80) == 0x80);    
    state.flags.cy = (answer > 0xff);    
    state.flags.p  = parity[answer & 0xff];    
}

/* calc flags and ac will be set */
void set_flags_new(uint16_t answer, uint8_t ac)
{
    set_flags(answer);
    state.flags.ac = ac;
}

/* called after an INC or DRC. It sets AC flag but not CY */
void set_flags_no_cy(uint8_t answer, uint8_t ac)
{
    state.flags.z  = ((answer & 0xff) == 0);    
    state.flags.s  = ((answer & 0x80) == 0x80);    
    state.flags.p  = parity[answer & 0xff];    
    state.flags.ac = ac; 
}

/* calculate parity array (parity set to 1 if number of ONES is even) */
void calc_parity_array()
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

/* pop the stack and move pc to popped address */
int ret()
{
    uint16_t addr = (uint16_t) memory[state.sp] +
                    (uint16_t) (memory[state.sp + 1] << 8);

    state.sp += 2;
    state.pc = addr;

    return 0;
}

/* add A register and b parameter and calculate flags */
void add(uint8_t b)
{
    /* calc result */
    uint16_t result = state.a + b;

    /* calc flags */
    set_flags_new(result, (((state.a & 0x0f) + (b & 0x0f)) > 9));
    
    /* save result into A register */
    state.a = (uint8_t) result & 0xff;

    return; 
}

/* add A register, b parameter and Carry flag, then calculate flags */
void adc(uint8_t b)
{
    /* calc result */
    uint16_t result = state.a + b + state.flags.cy;

    /* calc flags */
    set_flags_new(result, 
                  (((state.a & 0x0f) + (b & 0x0f) + state.flags.cy) > 9));

    /* save result into A register */
    state.a = (uint8_t) result & 0xff;

    return; 
}

/* subtract b parameter from A register and calculate flags */
void sub(uint8_t b)
{
    /* calc result */
    uint16_t result = state.a - b;

    /* calc flags */
    set_flags_new(result, (((state.a & 0x0f) - (b & 0x0f)) > 9));

    /* save result into A register */
    state.a = (uint8_t) result & 0xff;

    return;
}

/* subtract b parameter and Carry from A register and calculate flags */
void sbb(uint8_t b)
{
    /* calc result */
    uint16_t result = state.a - b - state.flags.cy;

    /* calc flags */
    set_flags_new(result, 
        (((state.a & 0x0f) - (b & 0x0f) - state.flags.cy) > 9));

    /* save result into A register */
    state.a = (uint8_t) result & 0xff;

    return;
}


/* push the current pc on the stack and move PC to the function addr */
int call(uint16_t addr)
{
    state.pc += 2;
    memory[state.sp - 1] = state.pc >> 8;
    memory[state.sp - 2] = state.pc & 0x00ff;

    state.sp -= 2;
    state.pc = addr;

    return 0;
}

int execute()
{
    unsigned char code = memory[state.pc];
    int b = 1;
    uint16_t *word, *word2;
    uint32_t answer32;
    uint16_t answer;
    uint16_t carry;
    uint16_t addr;
    uint8_t  xchg;
    uint8_t  *p;

    switch (code)
    {
        /* NOP       */
        case 0x00: break;                           

        /* LXI  B    */
        case 0x01: state.c = memory[state.pc + 1];   
                   state.b = memory[state.pc + 2];
                   b = 3;
                   break;

        /* STAX B    */
        case 0x02: memory[BC] = state.a;                    
                   break;                          

        /* INX  B    */
        case 0x03: answer = BC;                    
                   answer++;
                   state.b = answer / 256;
                   state.c = answer % 256;
                   break;        

        /* INR  B    */
        case 0x04: state.b++;                   
                   set_flags_no_cy(state.b, (state.b == 0x0a));
                   break;   

        /* DCR  B    */
        case 0x05: state.b--;                     
                   set_flags_no_cy(state.b, (state.b == 0x0f));
                   break;

        /* MVI  B    */
        case 0x06: state.b = memory[state.pc + 1];  
                   b = 2;
                   break;

        /* RLC       */
        case 0x07: carry = (state.a & 0x80) >> 7;   
                   state.a = state.a << 1; 
                   state.a |= carry;
                   state.flags.cy = carry;
                   break;

        /* NOP       */
        case 0x08: break;                          

        /* DAD  B    */
        case 0x09: answer32 = (uint32_t) HL + (uint32_t) BC;    
                   state.flags.cy = (answer32 > 0xffff);
                   answer32 &= 0xffff;
                   state.h = answer32 >> 8;
                   state.l = answer32 & 0x00ff;
                   break;

        /* LDAX B    */
        case 0x0A: state.a = memory[BC];          
                   break;

        /* DCX  B    */
        case 0x0B: answer = BC;                 
                   answer--;
                   state.b = answer / 256;
                   state.c = answer % 256;
                   break;

        /* INR  C    */
        case 0x0C: state.c++;                 
                   set_flags_no_cy(state.c - 1, state.c);
                   break;   

        /* DCR  C    */
        case 0x0D: state.c--;            
                   set_flags_no_cy(state.c + 1, state.c);
                   break;   

        /* MVI  C    */
        case 0x0E: state.c = memory[state.pc + 1]; 
                   b = 2;
                   break;

        /* RRC       */
        case 0x0F: carry = state.a & 0x01;       
                   state.a = state.a >> 1 | (carry << 7); 
                   state.flags.cy = carry;
                   break;

        /* NOP       */
        case 0x10: break;                           

        /* LXI  D    */
        case 0x11: state.e = memory[state.pc + 1];  
                   state.d = memory[state.pc + 2];
                   b = 3;
                   break;

        /* STAX D    */
        case 0x12: memory[DE] = state.a;            
                   break;

        /* INX  D    */
        case 0x13: answer = DE;                    
                   answer++;
                   state.d = answer / 256;
                   state.e = answer % 256;
                   break;

        /* INR  D    */
        case 0x14: state.d++;                
                   set_flags_no_cy(state.d - 1, state.d);
                   break;   

        /* DCR  D    */
        case 0x15: state.d--;              
                   set_flags_no_cy(state.d + 1, state.d);
                   break;   

        /* MVI  D    */
        case 0x16: state.d = memory[state.pc + 1];  
                   b = 2;
                   break;

        /* RAL       */
        case 0x17: carry = (state.a >> 7);       
                   state.a = state.a << 1;
                   state.a |= state.flags.cy;
                   state.flags.cy = carry;
                   break;

        /* NOP       */
        case 0x18: break;                    

        /* DAD  D    */
        case 0x19: answer32 = (uint32_t) HL + DE;   
                   state.flags.cy = (answer32 > 0xffff);
                   answer32 &= 0xffff;
                   state.h = answer32 >> 8;
                   state.l = answer32 & 0x00ff;
                   break;

        /* LDAX D    */
        case 0x1A: state.a = memory[DE];            
                   break;

        /* DCX  D    */
        case 0x1B: answer = DE;                    
                   answer--;
                   state.d = answer / 256;
                   state.e = answer % 256;
                   break;

        /* INR  E    */
        case 0x1C: state.e++;                  
                   set_flags_no_cy(state.e - 1, state.e);
                   break;

        /* DCR  E    */
        case 0x1D: state.e--;                       
                   set_flags_no_cy(state.e + 1, state.e);
                   break;

        /* MVI  E    */
        case 0x1E: state.e = memory[state.pc + 1];   
                   b = 2;
                   break;

        /* RAR       */
        case 0x1F: state.flags.cy = state.a & 0x01;  
                   carry = (state.a & 0x80);        
                   state.a = (state.a >> 1) | carry; 
                   break;

        /* RIM       */
        case 0x20: break;                        

        /* LXI  H    */
        case 0x21: state.l = memory[state.pc + 1];   
                   state.h = memory[state.pc + 2];
                   b = 3;
                   break;

        /* SHLD      */
        case 0x22: memory[ADDR] = state.l; 
                   memory[ADDR + 1] = state.h;
                   b = 3;
                   break;

        /* INX  H    */
        case 0x23: answer = HL;                   
                   answer++;
                   state.h = answer / 256;
                   state.l = answer % 256;
                   break;

        /* INR  H    */
        case 0x24: state.h++;                      
                   set_flags_no_cy(state.h - 1, state.h);
                   break;

        /* DCR  H    */
        case 0x25: state.h--;                       
                   set_flags_no_cy(state.h + 1, state.h);
                   break;

        /* MVI  H    */
        case 0x26: state.h = memory[state.pc + 1];   
                   b = 2;
                   break;

        /* DAA       */
        case 0x27: answer = state.a;

                   if ((answer & 0x0f) > 9 || state.flags.ac)
                   {
                       answer += 6;  
                       state.flags.ac = 1;
                   }
                   
                   if ((answer & 0xf0) >> 4 > 9 || state.flags.cy)
                       answer += 96;

                   /* update all the flags but the ac */
                   set_flags_new((uint16_t) answer, state.flags.ac);
                  
                   state.a = (uint8_t) answer & 0x00ff; 

                   break;                            

        /* NOP       */
        case 0x28: break;                           

        /* DAD  H    */
        case 0x29: answer32 = (uint32_t) (HL + HL);  
                   state.flags.cy = (answer32 > 0xffff);
                   answer32 &= 0xffff;
                   state.h = answer32 >> 8;
                   state.l = answer32 & 0x00ff;
                   break;

        /* LHLD      */
        case 0x2A: state.h = memory[ADDR + 1];    
                   state.l = memory[ADDR];
                   b = 3;
                   break;

        /* DCX  H    */
        case 0x2B: answer = HL;                   
                   answer--;
                   state.h = answer / 256;
                   state.l = answer % 256;
                   break;

        /* INR  L    */
        case 0x2C: state.l++;                       
                   set_flags_no_cy(state.l - 1, state.l);
                   break;

        /* DCR  L    */
        case 0x2D: state.l--;                      
                   set_flags_no_cy(state.l + 1, state.l);
                   break;

        /* MVI  L    */
        case 0x2E: state.l = memory[state.pc + 1];  
                   b = 2;
                   break;

        /* CMA  A    */
        case 0x2F: state.a = ~state.a;             
                   break;

        /* SIM       */
        case 0x30: break;                     
 
        /* LXI  SP   */
        case 0x31: state.sp = (uint16_t) memory[state.pc + 1];  
                   state.sp += (uint16_t) memory[state.pc + 2] << 8;
                   b = 3;
                   break;

        /* STA       */
        case 0x32: memory[ADDR] = state.a;          
                   b = 3;
                   break;

        /* INX  SP   */
        case 0x33: state.sp++;           
                   break;

        /* INR  M    */
        case 0x34: addr = (uint16_t) (state.h << 8) + state.l;  
                   memory[addr]++;
                   set_flags_no_cy(memory[addr] - 1, memory[addr]);
                   break;

        /* DCR  M    */
        case 0x35: addr = (uint16_t) (state.h << 8) + state.l;  
                   memory[addr]--;
                   set_flags_no_cy(memory[addr] + 1, memory[addr]);
                   break;

        /* MVI  M    */
        case 0x36: addr = (uint16_t) (state.h << 8) + state.l;   
                   memory[addr] = memory[state.pc + 1];
                   break;

        /* STC       */
        case 0x37: state.flags.cy = 1;              
                   break;

        /* NOP       */
        case 0x38: break;                          

        /* DAD  SP   */
        case 0x39: answer32 = (uint32_t) (HL + state.sp);  
                   state.flags.cy = (answer32 > 0xffff);
                   answer32 &= 0xffff;
                   state.h = answer32 >> 8;
                   state.l = answer32 & 0x00ff;
                   break;

        /* LDA       */
        case 0x3A: state.a = memory[ADDR];        
                   b = 3;
                   break;

        /* DCX  SP   */
        case 0x3B: state.sp--;                    
                   break;

        /* INR  A    */
        case 0x3C: state.a++;                      
                   set_flags_no_cy(state.a - 1, state.a);
                   break;

        /* DCR  A    */
        case 0x3D: state.a--;                
                   set_flags_no_cy(state.a - 1, state.a);
                   break;

        /* MVI  A   */
        case 0x3E: state.a = memory[state.pc + 1];   
                   b = 2;
                   break;

        /* ???      */
        case 0x3F: state.flags.cy = !state.flags.cy;

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
        case 0x46: state.b = memory[HL]; 
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
        case 0x4E: state.c = memory[HL]; 
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
        case 0x56: state.d = memory[HL]; 
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
        case 0x5E: state.e = memory[HL]; 
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
        case 0x66: state.h = memory[HL]; 
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
        case 0x6E: state.l = memory[HL]; 
                   break;  

        /* MOV  L,A  */
        case 0x6F: state.l = state.a; 
                   break;  

        /* MOV  M,B  */
        case 0x70: memory[HL] = state.b;
                   break;

        /* MOV  M,C  */
        case 0x71: memory[HL] = state.c;
                   break;

        /* MOV  M,D  */
        case 0x72: memory[HL] = state.d;
                   break;

        /* MOV  M,E  */
        case 0x73: memory[HL] = state.e;
                   break;

        /* MOV  M,H  */
        case 0x74: memory[HL] = state.h;
                   break;

        /* MOV  M,L  */
        case 0x75: memory[HL] = state.l;
                   break;

        /* HLT       */
        case 0x76: return 1;

        /* MOV  M,A  */
        case 0x77: memory[HL] = state.a;
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
        case 0x7E: state.a = memory[HL]; 
                   break;  

        /* MOV  A,A  */
        case 0x7F: state.a = state.a; 
                   break;  

        /* ADD  B    */
        case 0x80: add(state.b);
                   break;   

        /* ADD  C    */
        case 0x81: add(state.c);
                   break;   

        /* ADD  D    */
        case 0x82: add(state.d);
                   break;   

        /* ADD  E    */
        case 0x83: add(state.e);
                   break;   

        /* ADD  H    */
        case 0x84: add(state.h);
                   break;   

        /* ADD  L    */
        case 0x85: add(state.l);
                   break;   

        /* ADD  M    */
        case 0x86: add(memory[HL]); 
                   break;   

        /* ADD  A    */
        case 0x87: add(state.a);
                   break;   

        /* ADC  B    */
        case 0x88: adc(state.b); 
                   break;   

        /* ADC  C    */
        case 0x89: adc(state.c);
                   break;   

        /* ADC  D    */
        case 0x8a: adc(state.d);
                   break;   

        /* ADC  E    */
        case 0x8b: adc(state.e);
                   break;   

        /* ADC  H    */
        case 0x8c: adc(state.h); 
                   break;   

        /* ADC  L    */
        case 0x8d: adc(state.l);
                   break;   

        /* ADC  M    */
        case 0x8e: adc(memory[HL]);
                   break;   

        /* ADC  A    */
        case 0x8f: adc(state.a);
                   break;   

        /* SUB  B    */
        case 0x90: sub(state.b);
                   break;   

        /* SUB  C    */
        case 0x91: sub(state.c);
                   break;   

        /* SUB  D    */
        case 0x92: sub(state.d);
                   break;   

        /* SUB  E    */
        case 0x93: sub(state.e);
                   break;   

        /* SUB  H    */
        case 0x94: sub(state.h);
                   break;   

        /* SUB  L    */
        case 0x95: sub(state.l);
                   break;   

        /* SUB  M    */
        case 0x96: sub(memory[HL]);
                   break;   

        /* SUB  A    */
        case 0x97: sub(state.a);
                   break;   

        /* SBB  B    */
        case 0x98: sbb(state.b);
                   break;   

        /* SBB  C    */
        case 0x99: sbb(state.c);
                   break;   

        /* SBB  D    */
        case 0x9a: sbb(state.d);
                   break;   

        /* SBB  E    */
        case 0x9b: sbb(state.e);
                   break;   

        /* SBB  H    */
        case 0x9c: sbb(state.h);
                   break;   

        /* SBB  L    */
        case 0x9d: sbb(state.l);
                   break;   

        /* SBB  M    */
        case 0x9e: sbb(memory[HL]); 
                   break;   

        /* SBB  A    */
        case 0x9f: sbb(state.a); 
                   break;   

        /* ANA  B    */
        case 0xA0: state.a &= state.b;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* ANA  C    */
        case 0xA1: state.a &= state.c;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* ANA  D    */ 
        case 0xA2: state.a &= state.d;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* ANA  E    */
        case 0xA3: state.a &= state.e;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* ANA  H    */
        case 0xA4: state.a &= state.h;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* ANA  L    */
        case 0xA5: state.a &= state.l;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* ANA  M    */
        case 0xA6: state.a &= memory[HL];
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* ANA  A    */
        case 0xA7: state.a &= state.a;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* XRA  B    */
        case 0xA8: state.a ^= state.b;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* XRA  C    */
        case 0xA9: state.a ^= state.c;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* XRA  D    */
        case 0xAA: state.a ^= state.d;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* XRA  E    */
        case 0xAB: state.a ^= state.e;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* XRA  H    */
        case 0xAC: state.a ^= state.h;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* XRA  L    */
        case 0xAD: state.a ^= state.l;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* XRA  M    */
        case 0xAE: state.a ^= memory[HL];
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* XRA  A    */
        case 0xAF: state.a ^= state.a;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* ORA  B    */
        case 0xB0: state.a |= state.b;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* ORA  C    */
        case 0xB1: state.a |= state.c;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* ORA  D    */ 
        case 0xB2: state.a |= state.d;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* ORA  E    */
        case 0xB3: state.a |= state.e;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* ORA  H    */
        case 0xB4: state.a |= state.h;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* ORA  L    */
        case 0xB5: state.a |= state.l;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* ORA  M    */
        case 0xB6: state.a |= memory[HL];
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* ORA  A    */
        case 0xB7: state.a |= state.a;
                   set_flags_new((uint16_t) state.a, 0);
                   break;

        /* CMP  B    */
        case 0xB8: answer = (uint16_t) state.a - state.b;
                   set_flags(answer);
                   break;

        /* CMP  C    */
        case 0xB9: answer = (uint16_t) state.a - state.c;
                   set_flags(answer);
                   break;

        /* CMP  D    */
        case 0xBA: answer = (uint16_t) state.a - state.d;
                   set_flags(answer);
                   break;

        /* CMP  E    */
        case 0xBB: answer = (uint16_t) state.a - state.e;
                   set_flags(answer);
                   break;

        /* CMP  H    */
        case 0xBC: answer = (uint16_t) state.a - state.h;
                   set_flags(answer);
                   break;

        /* CMP  L    */
        case 0xBD: answer = (uint16_t) state.a - state.l;
                   set_flags(answer);
                   break;

        /* CMP  M    */
        case 0xBE: answer = (uint16_t) state.a - memory[HL];
                   set_flags(answer);
                   break;

        /* CMP  A    */
        case 0xBF: answer = (uint16_t) state.a - state.a;
                   set_flags(answer);
                   break;


        /* RNZ       */
        case 0xC0: if (state.flags.z == 0)
                       return ret();
                   break;

        /* POP  B    */
        case 0xC1: state.c = memory[state.sp]; 
                   state.b = memory[state.sp + 1];
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
                       return call(ADDR);

                   b = 3;
                   break;

        /* PUSH B    */
        case 0xC5: memory[state.sp - 1] = state.b;
                   memory[state.sp - 2] = state.c;
                   state.sp -= 2;
                   break;

        /* CALL addr */
        case 0xCD: 
		   if (ADDR == 5)    
		   {
			   if (state.c == 9)
			   {    
				   uint16_t offset = (state.d<<8) | (state.e);    
				   unsigned char *str = &memory[offset+3];      
				   while (*str != '$')
					   printf("%c", *str++);    
                                   printf("%c%02x%02x", *str, *(str+1),
                                                        *(str+2));

				   printf("\n");    
			   }    
			   else if (state.c == 2)    
			   {    
				   //saw this in the inspected code, never saw it called    
				   printf ("print char routine called\n");    
			   }    

                           return 1;

			   b = 3;

			   break;
		   }    
		   else if (ADDR == 0)    
		   {    
			   exit(0);    
		   }    

                   return call(ADDR);

        /* ADI       */
        case 0xC6: answer = (uint16_t) state.a + memory[state.pc + 1];
                   set_flags(answer);
                   state.a = (uint8_t) answer & 0xff; 
                   b = 2;
                   break;

        /* RZ        */
        case 0xC8: if (state.flags.z)
                       return ret();
                   break;

        /* RET       */
        case 0xC9: return ret();

        /* JZ        */
        case 0xCA: if (state.flags.z)
                   {
                       state.pc = ADDR;
                       return 0;
                   }

                   b = 3;
                   break;
        
        /* CZ        */
        case 0xCC: if (state.flags.z)
                       return call(ADDR);

                   b = 3;
                   break;
 
        /* ACI       */
        case 0xCE: answer = (uint16_t) state.a + 
                            memory[state.pc + 1] +
                            state.flags.cy;
                   set_flags(answer);
                   state.a = (uint8_t) answer & 0xff;
                   b = 2;
                   break;

        /* RNC       */
        case 0xD0: if (state.flags.cy == 0)
                       return ret();
                   break;

        /* POP  D    */
        case 0xD1: state.e = memory[state.sp]; 
                   state.d = memory[state.sp + 1];
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
                       return call(ADDR);

                   b = 3;
                   break;

        /* PUSH D    */
        case 0xD5: memory[state.sp - 1] = state.d;
                   memory[state.sp - 2] = state.e;
                   state.sp -= 2;
                   break;

        /* SUI       */
        case 0xD6: answer = (uint16_t) state.a - memory[state.pc + 1];
                   set_flags(answer);
                   state.a = (uint8_t) answer & 0xff;
                   b = 2;
                   break;

        /* RC        */
        case 0xD8: if (state.flags.cy)
                       return ret();
                   break;

        /* JC        */
        case 0xDA: if (state.flags.cy)
                   {
                       state.pc = ADDR;
                       return 0;
                   }

                   b = 3;
                   break;

        /* CC        */
        case 0xDC: if (state.flags.cy)
                       return call(ADDR);

                   b = 3;
                   break;

        /* SBI       */
        case 0xDE: answer = (uint16_t) state.a -
                            memory[state.pc + 1] -
                            state.flags.cy;
                   printf("SBI: A: %02x - MEM: %02x - ANSWER: %04x\n",
                          state.a, memory[state.pc + 1], answer);
                   set_flags(answer);
                   state.a = (uint8_t) answer & 0xff;
                   b = 2;
                   break;

        /* RPO       */
        case 0xE0: if (state.flags.p == 0)
                       return ret();
                   break;

        /* JPO       */
        case 0xE2: if (state.flags.p == 0)
                   {
                       state.pc = ADDR;
                       return 0;
                   }

                   b = 3;
                   break;

        /* CPO       */
        case 0xE4: if (state.flags.p == 0)
                       return call(ADDR);

                   b = 3;
                   break;

        /* ANI       */
        case 0xE6: state.a &= memory[state.pc + 1];
                   set_flags((uint16_t) state.a);
                   b = 2;                      
                   break;

        /* RPE       */
        case 0xE8: if (state.flags.p)
                       return ret();
                   break;

        /* POP  H    */
        case 0xE1: state.l = memory[state.sp]; 
                   state.h = memory[state.sp + 1];
                   state.sp += 2;
                   break;
    
        /* XTHL      */
        case 0xE3: xchg = memory[state.sp]; 
                   memory[state.sp] = state.l;
                   state.l = xchg;
                   xchg = memory[state.sp + 1];
                   memory[state.sp + 1] = state.h;
                   state.h = xchg;
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
                       return call(ADDR);

                   b = 3;
                   break;

        /* PUSH H    */
        case 0xE5: memory[state.sp - 1] = state.h;
                   memory[state.sp - 2] = state.l;
                   state.sp -= 2;
                   break;

        /* PCHL      */
        case 0xE9: state.pc = (uint16_t) state.h << 8;
                   state.pc += state.l;
                   return 0;

        /* JPE       */
        case 0xEA: if (state.flags.p)
                   {
                       state.pc = ADDR;
                       return 0;
                   }

                   b = 3;
                   break;

        /* XRI       */
        case 0xEE: answer = (uint16_t) state.a ^ memory[state.pc + 1];
                   set_flags(answer);
                   state.a = (uint8_t) answer;
                   b = 2;
                   break;

        /* RP        */
        case 0xF0: if (state.flags.s == 0)
                       return ret();
                   break;

        /* POP  PSW  */
        case 0xF1: p = (uint8_t *) &state.flags;
                   *p         = memory[state.sp];
                   state.a    = memory[state.sp + 1];
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
       
        /* CP        */
        case 0xF4: if (state.flags.p)
                       return call(ADDR);

                   b = 3;
                   break;
 
        /* PUSH PSW  */
        case 0xF5: p = (uint8_t *) &state.flags;
                   memory[state.sp - 1] = state.h;
                   memory[state.sp - 2] = *p;
                   state.sp -= 2;
                   break;

        /* ORI       */
        case 0xF6: answer = (uint16_t) state.a | memory[state.pc + 1];
                   set_flags(answer);
                   state.a = (uint8_t) answer;
                   b = 2;
                   break;

        /* RM        */
        case 0xF8: if (state.flags.s)
                       return ret();
                   break;

        /* JM        */
        case 0xFA: if (state.flags.s)
                   {
                       state.pc = ADDR;
                       return 0;
                   }

                   b = 3;
                   break;

        /* SPHL     */
        case 0xF9: state.sp = HL;
                   break;

        /* EI       */
        case 0xFB: state.int_enable = 1; 
                   break;

        /* CM        */
        case 0xFC: if (state.flags.s)
                       return call(ADDR);

                   b = 3;
                   break;

        /* CPI      */
        case 0xFE: answer = (uint16_t) state.a - memory[state.pc + 1];
                   set_flags(answer);
                   b = 2;                      
                   break;

        default:
            printf("UNKNOWN OP CODE: %02x\n", code);
            return 1;
    }

    state.pc += b;
    return 0;
}



int disassemble(unsigned char *codebuffer, int pc)
{
    unsigned char code = codebuffer[pc];
    int b = 1;

    printf("%04x ", pc);
    printf("OP: %02x ", code);

    switch (code)
    {
        case 0x00: printf("NOP"); break;
        case 0x01: printf("LXI  B,#$%02x%02x", codebuffer[pc + 2], 
                                               codebuffer[pc + 1]); 
                   b = 3;
                   break;
        case 0x02: printf("STAX B"); break;
        case 0x03: printf("INX  B"); break;
        case 0x04: printf("INR  B"); break;
        case 0x05: printf("DCR  B"); break;
        case 0x06: printf("MVI  B,#0x%02x", codebuffer[pc + 1]); 
                   b = 2;
                   break;
        case 0x07: printf("RLC"); break;
        case 0x08: printf("NOP"); break;
        case 0x09: printf("DAD  B"); break;  /* hl <- hl+bc   */
        case 0x0A: printf("LDAX B"); break;  /* a  <- bc      */
        case 0x0B: printf("DCX  B"); break;  /* bc <- bc-1    */
        case 0x0C: printf("INR  C"); break;  /* c  <- c+1     */
        case 0x0D: printf("DRC  C"); break;  /* c  <- c-1     */
        case 0x0E: printf("MVI  C,#%02x", codebuffer[pc + 1]); 
                   b = 2;
                   break;                    /* c  <- byte 2  */
        case 0x0F: printf("RRC  "); break;   /* a  <- a>>1; bit 7=prev bit 0;
                                                            CY   =prev bit 0 */ 
        case 0x10: printf("NOP"); break;
        case 0x11: printf("LXI  D,#$%02x%02x", codebuffer[pc + 2], 
                                               codebuffer[pc + 1]); 
                   b = 3;
                   break;
        case 0x12: printf("STAX D"); break;  /* de <- a       */
        case 0x13: printf("INX  D"); break;  /* de <- de+1    */
        case 0x14: printf("INR  D"); break;  /* d  <- d+1     */
        case 0x15: printf("DCR  D"); break;  /* d  <- d-1     */
        case 0x16: printf("MVI  D,#%02x", codebuffer[pc + 1]); 
                   b = 2;
                   break;                    /* c  <- byte 2  */
        case 0x17: printf("RAL  "); break;   /* a  <- a<<1; bit 0=prev CY;
                                                            CY   =prev bit 7 */ 
        case 0x18: printf("NOP"); break;
        case 0x19: printf("DAD  D"); break;    /* hl <- hl+de   */
        case 0x1a: printf("LDAX D"); break;    /* a  <- de      */
        case 0x1b: printf("DCX  D"); break;    /* de <- de-1    */
        case 0x1c: printf("INR  E"); break;    /* e  <- e+1     */
        case 0x1d: printf("DCR  E"); break;    /* e  <- e-1     */
        case 0x1e: printf("MVI  E,#%02x", codebuffer[pc + 1]); 
                   b = 2;
                   break;                      /* e  <- byte 2  */
        case 0x1f: printf("RAR  "); break;     /* a  <- a>>1;      */
                                               /* bit 7=prev bit 7;*/
                                               /* CY   =prev bit 0 */ 
        case 0x20: printf("RIM"); break;       /* special       */
        case 0x21: printf("LXI  H,#$%02x%02x", codebuffer[pc + 2], 
                                               codebuffer[pc + 1]); 
                   b = 3;
                   break;
        case 0x22: printf("SHLD $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]);   
                                               /* addr[0] <- l
                                                  addr[1] <- h  */ 
                   b = 3;
                   break;
        case 0x23: printf("INX  H"); break;    /* hl <- hl+1    */
        case 0x24: printf("INR  H"); break;    /* h  <- h+1    */
        case 0x25: printf("DCR  H"); break;    /* h  <- h-1     */
        case 0x26: printf("MVI  H,#%02x", codebuffer[pc + 1]); 
                   b = 2;
                   break;                      /* h  <- byte 2  */
        case 0x27: printf("DAA"); break;       /* special       */
        case 0x28: printf("NOP"); break;
        case 0x29: printf("DAD  H"); break;    /* hl <- hl+h    */
        case 0x2a: printf("LHLD $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]);   
                                               /* l <- addr[0] 
                                                  h <- addr[1]  */ 
                   b = 3;
                   break;
        case 0x2b: printf("DCX  H"); break;    /* hl <- hl-1    */
        case 0x2c: printf("INR  L"); break;    /* l  <- l+1     */
        case 0x2d: printf("DCR  L"); break;    /* l  <- l-1     */
        case 0x2e: printf("MVI  L,#%02x", codebuffer[pc + 1]); 
                   b = 2;
                   break;                      /* l  <- byte 2   */
        case 0x2f: printf("CMA  A"); break;    /* a  <- !a       */
        case 0x30: printf("SIM"); break;       /* special        */
        case 0x31: printf("LXI  SP,#$%02x%02x", codebuffer[pc + 2], 
                                                codebuffer[pc + 1]); 
                   b = 3;                      /* sp.hi <- byte 3 */
                                               /* sp.lo <- byte 2 */
                   break;
        case 0x32: printf("STA  $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]);   
                                               /* addr <- a      */
                   b = 3;
                   break;
        case 0x33: printf("INX  SP"); break;   /* sp  <- sp+1    */
        case 0x34: printf("INR  M"); break;    /* *hl <- *hl+1   */
        case 0x35: printf("DCR  M"); break;    /* *hl <- *hl-1   */
        case 0x36: printf("MVI  M,#%02x", codebuffer[pc + 1]); 
                   b = 2;
                   break;                      /* *hl <- byte 2  */
        case 0x37: printf("STC"); break;       /* cy  <- 1       */
        case 0x38: printf("NOP"); break;
        case 0x39: printf("DAD  SP"); break;   /* hl <- hl+sp    */
        case 0x3a: printf("LDA  $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]);   
                                               /* a <- *addr     */
                   b = 3;
                   break;
        case 0x3b: printf("DCX  SP"); break;   /* sp <- sp-1     */
        case 0x3c: printf("INR  A"); break;    /* a  <- a+1      */
        case 0x3d: printf("DCR  A"); break;    /* a  <- a-1      */
        case 0x3e: printf("MVI  A,#0x%02x", codebuffer[pc + 1]); 
                   b = 2;
                   break;                      /* a  <- byte 2   */
        case 0x3f: printf("CMC  CY"); break;   /* cy <- !cy      */
        case 0x40: printf("MOV  B,B"); break;  /* b  <- b        */
        case 0x41: printf("MOV  B,C"); break;  /* b  <- c        */
        case 0x42: printf("MOV  B,D"); break;  /* b  <- d        */
        case 0x43: printf("MOV  B,E"); break;  /* b  <- e        */
        case 0x44: printf("MOV  B,H"); break;  /* b  <- h        */
        case 0x45: printf("MOV  B,L"); break;  /* b  <- l        */
        case 0x46: printf("MOV  B,M"); break;  /* b  <- *hl      */
        case 0x47: printf("MOV  B,A"); break;  /* b  <- a        */
        case 0x48: printf("MOV  C,B"); break;  /* c  <- b        */
        case 0x49: printf("MOV  C,C"); break;  /* c  <- c        */
        case 0x4a: printf("MOV  C,D"); break;  /* c  <- d        */
        case 0x4b: printf("MOV  C,E"); break;  /* c  <- e        */
        case 0x4c: printf("MOV  C,H"); break;  /* c  <- h        */
        case 0x4d: printf("MOV  C,L"); break;  /* c  <- l        */
        case 0x4e: printf("MOV  C,M"); break;  /* c  <- *hl      */
        case 0x4f: printf("MOV  C,A"); break;  /* c  <- a        */
        case 0x50: printf("MOV  D,B"); break;  /* d  <- b        */
        case 0x51: printf("MOV  D,C"); break;  /* d  <- c        */
        case 0x52: printf("MOV  D,D"); break;  /* d  <- d        */
        case 0x53: printf("MOV  D,E"); break;  /* d  <- e        */
        case 0x54: printf("MOV  D,H"); break;  /* d  <- h        */
        case 0x55: printf("MOV  D,L"); break;  /* d  <- l        */
        case 0x56: printf("MOV  D,M"); break;  /* d  <- *hl      */
        case 0x57: printf("MOV  D,A"); break;  /* d  <- a        */
        case 0x58: printf("MOV  E,B"); break;  /* e  <- b        */
        case 0x59: printf("MOV  E,C"); break;  /* e  <- c        */
        case 0x5a: printf("MOV  E,D"); break;  /* e  <- d        */
        case 0x5b: printf("MOV  E,E"); break;  /* e  <- e        */
        case 0x5c: printf("MOV  E,H"); break;  /* e  <- h        */
        case 0x5d: printf("MOV  E,L"); break;  /* e  <- l        */
        case 0x5e: printf("MOV  E,M"); break;  /* e  <- *hl      */
        case 0x5f: printf("MOV  E,A"); break;  /* e  <- a        */
        case 0x60: printf("MOV  H,B"); break;  /* h  <- b        */
        case 0x61: printf("MOV  H,C"); break;  /* h  <- c        */
        case 0x62: printf("MOV  H,D"); break;  /* h  <- d        */
        case 0x63: printf("MOV  H,E"); break;  /* h  <- e        */
        case 0x64: printf("MOV  H,H"); break;  /* h  <- h        */
        case 0x65: printf("MOV  H,L"); break;  /* h  <- l        */
        case 0x66: printf("MOV  H,M"); break;  /* h  <- *hl      */
        case 0x67: printf("MOV  H,A"); break;  /* h  <- a        */
        case 0x68: printf("MOV  L,B"); break;  /* l  <- b        */
        case 0x69: printf("MOV  L,C"); break;  /* l  <- c        */
        case 0x6A: printf("MOV  L,D"); break;  /* l  <- d        */
        case 0x6B: printf("MOV  L,E"); break;  /* l  <- e        */
        case 0x6C: printf("MOV  L,H"); break;  /* l  <- h        */
        case 0x6D: printf("MOV  L,L"); break;  /* l  <- l        */
        case 0x6E: printf("MOV  L,M"); break;  /* l  <- *hl      */
        case 0x6F: printf("MOV  L,A"); break;  /* l  <- a        */
        case 0x70: printf("MOV  M,B"); break;  /* *hl<- b        */
        case 0x71: printf("MOV  M,C"); break;  /* *hl<- c        */
        case 0x72: printf("MOV  M,D"); break;  /* *hl<- d        */
        case 0x73: printf("MOV  M,E"); break;  /* *hl<- e        */
        case 0x74: printf("MOV  M,H"); break;  /* *hl<- h        */
        case 0x75: printf("MOV  M,L"); break;  /* *hl<- l        */
        case 0x76: printf("HLT"); break;       /* special        */
        case 0x77: printf("MOV  M,A"); break;  /* *hl<- a        */
        case 0x78: printf("MOV  A,B"); break;  /* a  <- b        */
        case 0x79: printf("MOV  A,C"); break;  /* a  <- c        */
        case 0x7a: printf("MOV  A,D"); break;  /* a  <- d        */
        case 0x7b: printf("MOV  A,E"); break;  /* a  <- e        */
        case 0x7c: printf("MOV  A,H"); break;  /* a  <- h        */
        case 0x7d: printf("MOV  A,L"); break;  /* a  <- l        */
        case 0x7e: printf("MOV  A,M"); break;  /* a  <- *hl      */
        case 0x7f: printf("MOV  A,A"); break;  /* a  <- a        */
        case 0x80: printf("ADD  B"); break;    /* a  <- a + b    */
        case 0x81: printf("ADD  C"); break;    /* a  <- a + c    */
        case 0x82: printf("ADD  D"); break;    /* a  <- a + d    */
        case 0x83: printf("ADD  E"); break;    /* a  <- a + e    */
        case 0x84: printf("ADD  H"); break;    /* a  <- a + h    */
        case 0x85: printf("ADD  L"); break;    /* a  <- a + l    */
        case 0x86: printf("ADD  M"); break;    /* a  <- a + *hl  */
        case 0x87: printf("ADD  A"); break;    /* a  <- a + a + cy   */
        case 0x88: printf("ADC  B"); break;    /* a  <- a + b + cy   */
        case 0x89: printf("ADC  C"); break;    /* a  <- a + c + cy   */
        case 0x8a: printf("ADC  D"); break;    /* a  <- a + d + cy   */
        case 0x8b: printf("ADC  E"); break;    /* a  <- a + e + cy   */
        case 0x8c: printf("ADC  H"); break;    /* a  <- a + h + cy   */
        case 0x8d: printf("ADC  L"); break;    /* a  <- a + l + cy   */
        case 0x8e: printf("ADC  M"); break;    /* a  <- a + *hl + cy */
        case 0x8f: printf("ADC  A"); break;    /* a  <- a + a + cy   */
        case 0x90: printf("SUB  B"); break;    /* a  <- a - b    */
        case 0x91: printf("SUB  C"); break;    /* a  <- a - c    */
        case 0x92: printf("SUB  D"); break;    /* a  <- a - d    */
        case 0x93: printf("SUB  E"); break;    /* a  <- a - e    */
        case 0x94: printf("SUB  H"); break;    /* a  <- a - h    */
        case 0x95: printf("SUB  L"); break;    /* a  <- a - l    */
        case 0x96: printf("SUB  M"); break;    /* a  <- a - *hl  */
        case 0x97: printf("SUB  A"); break;    /* a  <- a - a    */
        case 0x98: printf("SBB  B"); break;    /* a  <- a - b - cy   */
        case 0x99: printf("SBB  C"); break;    /* a  <- a - c - cy   */
        case 0x9a: printf("SBB  D"); break;    /* a  <- a - d - cy   */
        case 0x9b: printf("SBB  E"); break;    /* a  <- a - e - cy   */
        case 0x9c: printf("SBB  H"); break;    /* a  <- a - h - cy   */
        case 0x9d: printf("SBB  L"); break;    /* a  <- a - l - cy   */
        case 0x9e: printf("SBB  M"); break;    /* a  <- a - *hl - cy */
        case 0x9f: printf("SBB  A"); break;    /* a  <- a - a - cy   */
        case 0xA0: printf("ANA  B"); break;    /* a  <- a & b    */
        case 0xA1: printf("ANA  C"); break;    /* a  <- a & c    */
        case 0xA2: printf("ANA  D"); break;    /* a  <- a & d    */
        case 0xA3: printf("ANA  E"); break;    /* a  <- a & e    */
        case 0xA4: printf("ANA  H"); break;    /* a  <- a & h    */
        case 0xA5: printf("ANA  L"); break;    /* a  <- a & l    */
        case 0xA6: printf("ANA  M"); break;    /* a  <- a & *hl  */
        case 0xA7: printf("ANA  A"); break;    /* a  <- a & a    */
        case 0xA8: printf("XRA  B"); break;    /* a  <- a ^ b    */
        case 0xA9: printf("XRA  C"); break;    /* a  <- a ^ c    */
        case 0xAA: printf("XRA  D"); break;    /* a  <- a ^ d    */
        case 0xAB: printf("XRA  E"); break;    /* a  <- a ^ e    */
        case 0xAC: printf("XRA  H"); break;    /* a  <- a ^ h    */
        case 0xAD: printf("XRA  L"); break;    /* a  <- a ^ l    */
        case 0xAE: printf("XRA  M"); break;    /* a  <- a ^ *hl  */
        case 0xAF: printf("XRA  A"); break;    /* a  <- a ^ a    */
        case 0xB0: printf("ORA  B"); break;    /* a  <- a | b    */
        case 0xB1: printf("ORA  C"); break;    /* a  <- a | c    */
        case 0xB2: printf("ORA  D"); break;    /* a  <- a | d    */
        case 0xB3: printf("ORA  E"); break;    /* a  <- a | e    */
        case 0xB4: printf("ORA  H"); break;    /* a  <- a | h    */
        case 0xB5: printf("ORA  L"); break;    /* a  <- a | l    */
        case 0xB6: printf("ORA  M"); break;    /* a  <- a | *hl  */
        case 0xB7: printf("ORA  A"); break;    /* a  <- a | a    */
        case 0xB8: printf("CMP  B"); break;    /* a - b          */
        case 0xB9: printf("CMP  C"); break;    /* a - c          */
        case 0xBA: printf("CMP  D"); break;    /* a - d          */
        case 0xBB: printf("CMP  E"); break;    /* a - e          */
        case 0xBC: printf("CMP  H"); break;    /* a - h          */
        case 0xBD: printf("CMP  L"); break;    /* a - l          */
        case 0xBE: printf("CMP  M"); break;    /* a - *hl        */
        case 0xBF: printf("CMP  A"); break;    /* a - a          */


        case 0xC0: printf("RNZ"); break;       /* if NZ, RET       */
        case 0xC1: printf("POP  B"); break;    /* c <- *(sp)       */
                                               /* b <- *(sp+1)     */ 
                                               /* sp <- sp + 2     */
        case 0xC2: printf("JNZ  $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]); 
                   b = 3;                      /* if NZ, JMP       */
                   break;
        case 0xC3: printf("JMP  $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]); 
                   b = 3;
                   break;
        case 0xC4: printf("CNZ  $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]); 
                   b = 3;                      /* if NZ, CALL      */
                   break;
        case 0xC5: printf("PUSH B"); break;    /* *(sp-2) <- c     */
                                               /* *(sp-1) <- b     */ 
                                               /* sp <- sp - 2     */
        case 0xC6: printf("ADI  #%02x", codebuffer[pc + 1]); 
                   b = 2;
                   break;                      /* a  <- a + byte   */
        case 0xC7: printf("RST  0"); break;    /* jmp to $0        */
        case 0xC8: printf("RZ"); break;        /* if Z, RET        */
        case 0xC9: printf("RET"); break;       /* PC.lo <- *sp     */
                                               /* PC.hi <- *(sp+1) */        
                                               /* sp <- sp+2       */ 
        case 0xCA: printf("JZ   $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]); 
                   b = 3;                      /* if Z,JMP to addr */
                   break;
        case 0xCC: printf("CZ   $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]); 
                   b = 3;                      /* if Z,CALL addr   */
                   break;
        case 0xCD: printf("CALL $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]); 
                   b = 3;                      /* *(sp-1) <- PC.hi */
                                               /* *(sp-2) <- PC.lo */
                                               /* sp <- sp-2       */
                                               /* pc <- addr       */
                   break;
        case 0xCE: printf("ACI  #%02x", codebuffer[pc + 1]); 
                   b = 2;                      /* a <- a + data + cy */
                   break;        
        case 0xCF: printf("RST  1"); break;    /* jmp to $8        */
        case 0xD0: printf("RNC"); break;       /* if NCY, RET      */
        case 0xD1: printf("POP  D"); break;    /* e <- *(sp)       */
                                               /* d <- *(sp+1)     */ 
                                               /* sp <- sp + 2     */
        case 0xD2: printf("JNC  $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]); 
                   b = 3;                      /* if NCY, JMP addr */
                   break;
        case 0xD3: printf("OUT  #%02x", codebuffer[pc + 1]);
                   b = 2;                      /* special          */
                   break;
        case 0xD4: printf("CNC  $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]); 
                   b = 3;                      /* if NCY,CALL addr */
                   break;
        case 0xD5: printf("PUSH D"); break;    /* *(sp-2) <- e     */
                                               /* *(sp-1) <- d     */ 
                                               /* sp <- sp - 2     */
        case 0xD6: printf("SUI  #%02x", codebuffer[pc + 1]); 
                   b = 2;                      /* a <- a - data    */
                   break;        
        case 0xD7: printf("RST  2"); break;    /* call $10         */
        case 0xD8: printf("RC"); break;        /* if CY,RET        */
        case 0xDA: printf("JC   $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]);
                   b = 3;                      /* if CY, PC<-addr  */
                   break;
        case 0xDB: printf("IN   #%02x", codebuffer[pc + 1]);
                   b = 2;                      /* special          */
                   break;
        case 0xDC: printf("CC   $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]);
                   b = 3;                      /* if CY,CALL addr  */
                   break;
        case 0xDE: printf("SBI  #%02x", codebuffer[pc + 1]);
                   b = 2;                      /* a <- a-data-CY   */
                   break;
        case 0xDF: printf("RST  3"); break;    /* call $18         */
        case 0xE0: printf("RPO"); break;       /* if PO, RET       */
        case 0xE1: printf("POP  H"); break;    /* l <- *(sp)       */
                                               /* h <- *(sp+1)     */ 
                                               /* sp <- sp + 2     */
        case 0xE2: printf("JPO  $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]);
                   b = 3;                      /* if PO,JMP addr   */
                   break;
        case 0xE3: printf("XTHL"); break;      /* l<->*sp; h<->*sp+1 */
        case 0xE4: printf("CPO  $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]);
                   b = 3;                      /* if PO,CALL addr  */
                   break;
        case 0xE5: printf("PUSH H"); break;    /* *(sp-2) <- l     */
                                               /* *(sp-1) <- h     */ 
                                               /* sp <- sp - 2     */
        case 0xE6: printf("ANI  #%02x", codebuffer[pc + 1]);
                   b = 2;                      /* a <- a & data    */
                   break;
        case 0xE7: printf("RST  4"); break;    /* call $20         */
        case 0xE8: printf("RPE"); break;       /* if PE,RET        */
        case 0xE9: printf("PCHL"); break;      /* PC.hi <- h; PC.lo <- l */
        case 0xEA: printf("JPE  $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]);
                   b = 3;                      /* if PE, PC<-addr  */
                   break;
        case 0xEB: printf("XCHG"); break;      /* h <-> d; l <-> e */ 
        case 0xEC: printf("CPE  $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]);
                   b = 3;                      /* if PE,CALL addr  */
                   break;
        case 0xEE: printf("XRI  #%02x", codebuffer[pc + 1]);
                   b = 2;                      /* a <- a ^ data    */
                   break;
        case 0xEF: printf("RST  5"); break;    /* call $28         */

        case 0xF0: printf("RP"); break;        /* if P, RET        */
        case 0xF1: printf("POP  PSW"); break;  /* flags <- *(sp)   */
                                               /* a <- *(sp+1)     */ 
                                               /* sp <- sp + 2     */
        case 0xF2: printf("JP   $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]);
                   b = 3;                      /* if P,JMP addr    */
                   break;
        case 0xF3: printf("DI"); break;        /* special          */
        case 0xF4: printf("CP   $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]);
                   b = 3;                      /* if P,CALL addr   */
                   break;
        case 0xF5: printf("PUSH PSW"); break;  /* *(sp-2) <- flags */
                                               /* *(sp-1) <- a     */ 
                                               /* sp <- sp - 2     */
        case 0xF6: printf("ORI  #%02x", codebuffer[pc + 1]);
                   b = 2;                      /* a <- a | data    */
                   break;
        case 0xF7: printf("RST  6"); break;    /* call $30         */
        case 0xF8: printf("RM"); break;        /* if M,RET         */
        case 0xF9: printf("SPHL"); break;      /* sp <- hl         */
        case 0xFA: printf("JM   $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]);
                   b = 3;                      /* if M, JMP addr   */
                   break;
        case 0xFB: printf("EI"); break;        /* special          */
        case 0xFC: printf("CM   $%02x%02x", codebuffer[pc + 2],
                                            codebuffer[pc + 1]);
                   b = 3;                      /* if M,CALL addr   */
                   break;
        case 0xFE: printf("CPI  #%02x", codebuffer[pc + 1]);
                   b = 2;                      /* a - data         */
                   break;
        case 0xFF: printf("RST  7"); break;    /* call $38         */



        default:
            printf("UNKNOWN: %02x", code);
    }

    printf("\n");

    return b;
}

int main(int argc, char **argv)
{
    FILE *f= fopen(argv[1], "rb");    

    if (f==NULL)    
    {    
        printf("error: Couldn't open %s\n", argv[1]);    
        return 1;    
    }

    //Get the file size and read it into a memory buffer    
    fseek(f, 0L, SEEK_END);    
    int fsize = ftell(f);    
    fseek(f, 0L, SEEK_SET);    

    /* wipe all the structs */
    bzero(&state, sizeof(state_t));
    bzero(memory, sizeof(memory));

    /* setup parity array */
    calc_parity_array();

    /* read the 8192 bytes of ROM image into first 8192 of memory */
    fread(&memory[0x0100], fsize, 1, f);    
    fclose(f);   

    int pc = 0;
    int debug = 0;

    /* debug purposes */
    while (debug && pc < 8192)
    {
        pc += disassemble(memory, pc);
    }

    if (debug == 1)
        return;

    int i = 0;

    state.pc = 0x0100;

    memory[368] = 0x7;

    /* skip DAA test */
/*    memory[0x59c] = 0xc3; 
    memory[0x59d] = 0xc2;    
    memory[0x59e] = 0x05; 
*/

    /* running stuff! */
    for (i=0;i<1500000;i++)
    {
        disassemble(memory, state.pc);

        if (execute())
        {
            printf("ciclo %d\n",i);
            break;
        }
    }

    printf("cicli ESEGUITI: %d\n", i);

    print_state();
    print_video_ram();

    return 0;
}
