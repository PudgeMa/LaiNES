/*
// shatbox (C) 2002 Matthew Conte (matt@conte.com)
**
**
** nes6502.c
**
** NES custom 6502 (2A03) CPU implementation
**
** $Id: $
*/

#include <stdio.h>
#include "nes6502.h"
#include "dis6502.h"

//#define  NES6502_DISASM
//#define  NES6502_RMW_EXACT
#define  NES6502_JUMPTABLE

#ifdef NES6502_DISASM
#define DISASM_INSTRUCTION(_PC, _P, _A, _X, _Y, _S)   printf(nes6502_disasm(_PC, _P, _A, _X, _Y, _S))
#else
#define DISASM_INSTRUCTION(_PC, _P, _A, _X, _Y, _S)
#endif

#ifdef NES6502_RMW_EXACT
#define  RMW_STEP(func, address, val)  func((address), (val))
#else /* !NES6502_RMW_EXACT */
#define  RMW_STEP(func, address, val)
#endif /* !NES6502_RMW_EXACT */

#ifndef MIN
#define  MIN(a,b)    (((a) < (b)) ? (a) : (b))
#endif

#define  INLINE  static inline

/* P (flag) register bitmasks */
#define  N_FLAG         0x80
#define  V_FLAG         0x40
#define  R_FLAG         0x20  /* Reserved, always 1 */
#define  B_FLAG         0x10
#define  D_FLAG         0x08
#define  I_FLAG         0x04
#define  Z_FLAG         0x02
#define  C_FLAG         0x01

/* Vector addresses */
#define  NMI_VECTOR     0xfffa
#define  RESET_VECTOR   0xfffc
#define  IRQ_VECTOR     0xfffe

/* cycle counts for interrupts */
#define  INT_CYCLES     7
#define  RESET_CYCLES   6

#define  NMI_MASK       0x01
#define  IRQ_MASK       0x02

/* Stack is located on 6502 page 1 */
#define  STACK_OFFSET   0x0100


#define  GET_GLOBAL_REGS() \
{ \
   PC = cpu.pc_reg; \
   A = cpu.a_reg; \
   X = cpu.x_reg; \
   Y = cpu.y_reg; \
   SCATTER_FLAGS(cpu.p_reg); \
   S = cpu.s_reg; \
}

#define  STORE_LOCAL_REGS() \
{ \
   cpu.pc_reg = PC; \
   cpu.a_reg = A; \
   cpu.x_reg = X; \
   cpu.y_reg = Y; \
   cpu.p_reg = COMBINE_FLAGS(); \
   cpu.s_reg = S; \
}


#define  ADD_CYCLES(x) \
{ \
   cpu.total_cycles += (x); \
   cpu.remaining_cycles -= (x); \
}

/*
** Check to see if an index reg addition overflowed to next page
*/
#define PAGE_CROSS_CHECK(addr, reg) \
{ \
   if ((reg) > (uint8_t) (addr)) \
      ADD_CYCLES(1); \
}

#define DUMMY_READ(value)

/*
** Addressing mode macros
*/

/* Immediate */
#define IMMEDIATE_BYTE(value) \
{ \
   value = bank_readbyte(PC++); \
}

/* Absolute */
#define ABSOLUTE_ADDR(address) \
{ \
   address = bank_readword(PC); \
   PC += 2; \
}

#define ABSOLUTE(address, value) \
{ \
   ABSOLUTE_ADDR(address); \
   value = mem_readbyte(address); \
}

#define ABSOLUTE_BYTE(value) \
{ \
   ABSOLUTE(temp, value); \
}

/* Absolute indexed X */
#define ABS_IND_X_ADDR(address) \
{ \
   ABSOLUTE_ADDR(address); \
   address = (address + X) & 0xffff; \
}

#define ABS_IND_X(address, value) \
{ \
   ABS_IND_X_ADDR(address); \
   value = mem_readbyte(address); \
}

#define ABS_IND_X_BYTE(value) \
{ \
   ABS_IND_X(temp, value); \
}

/* special page-cross check version for read instructions */
#define ABS_IND_X_BYTE_READ(value) \
{ \
   ABS_IND_X_ADDR(temp); \
   PAGE_CROSS_CHECK(temp, X); \
   value = mem_readbyte(temp); \
}

/* Absolute indexed Y */
#define ABS_IND_Y_ADDR(address) \
{ \
   ABSOLUTE_ADDR(address); \
   address = (address + Y) & 0xffff; \
}

#define ABS_IND_Y(address, value) \
{ \
   ABS_IND_Y_ADDR(address); \
   value = mem_readbyte(address); \
}

#define ABS_IND_Y_BYTE(value) \
{ \
   ABS_IND_Y(temp, value); \
}

/* special page-cross check version for read instructions */
#define ABS_IND_Y_BYTE_READ(value) \
{ \
   ABS_IND_Y_ADDR(temp); \
   PAGE_CROSS_CHECK(temp, Y); \
   value = mem_readbyte(temp); \
}

/* Zero-page */
#define ZERO_PAGE_ADDR(address) \
{ \
   IMMEDIATE_BYTE(address); \
}

#define ZERO_PAGE(address, value) \
{ \
   ZERO_PAGE_ADDR(address); \
   value = ZP_READBYTE(address); \
}

#define ZERO_PAGE_BYTE(value) \
{ \
   ZERO_PAGE(btemp, value); \
}

/* Zero-page indexed X */
#define ZP_IND_X_ADDR(address) \
{ \
   ZERO_PAGE_ADDR(address); \
   address += X; \
}

#define ZP_IND_X(address, value) \
{ \
   ZP_IND_X_ADDR(address); \
   value = ZP_READBYTE(address); \
}

#define ZP_IND_X_BYTE(value) \
{ \
   ZP_IND_X(btemp, value); \
}

/* Zero-page indexed Y */
/* Not really an adressing mode, just for LDx/STx */
#define ZP_IND_Y_ADDR(address) \
{ \
   ZERO_PAGE_ADDR(address); \
   address += Y; \
}

#define ZP_IND_Y_BYTE(value) \
{ \
   ZP_IND_Y_ADDR(btemp); \
   value = ZP_READBYTE(btemp); \
}  

/* Indexed indirect */
#define INDIR_X_ADDR(address) \
{ \
   ZERO_PAGE_ADDR(btemp); \
   btemp += X; \
   address = zp_readword(btemp); \
}

#define INDIR_X(address, value) \
{ \
   INDIR_X_ADDR(address); \
   value = mem_readbyte(address); \
} 

#define INDIR_X_BYTE(value) \
{ \
   INDIR_X(temp, value); \
}

/* Indirect indexed */
#define INDIR_Y_ADDR(address) \
{ \
   ZERO_PAGE_ADDR(btemp); \
   address = (zp_readword(btemp) + Y) & 0xffff; \
}

#define INDIR_Y(address, value) \
{ \
   INDIR_Y_ADDR(address); \
   value = mem_readbyte(address); \
} 

#define INDIR_Y_BYTE(value) \
{ \
   INDIR_Y(temp, value); \
}

/* special page-cross check version for read instructions */
#define INDIR_Y_BYTE_READ(value) \
{ \
   INDIR_Y_ADDR(temp); \
   PAGE_CROSS_CHECK(temp, Y); \
   value = mem_readbyte(temp); \
}



/* Stack push/pull */
#define  PUSH(value)             cpu.mem_page[0][STACK_OFFSET + S--] = (uint8_t) (value)
#define  PULL()                  cpu.mem_page[0][STACK_OFFSET + ++S]


/*
** flag register helper macros
*/

/* Theory: Z and N flags are set in just about every
** instruction, so we will just store the value in those
** flag variables, and mask out the irrelevant data when
** we need to check them (branches, etc).  This makes the
** zero flag only really be 'set' when z_flag == 0.
** The rest of the flags are stored as true booleans.
*/

/* Scatter flags to separate variables */
#define  SCATTER_FLAGS(value) \
{ \
   n_flag = (value) & N_FLAG; \
   v_flag = (value) & V_FLAG; \
   d_flag = (value) & D_FLAG; \
   i_flag = (value) & I_FLAG; \
   z_flag = (0 == ((value) & Z_FLAG)); \
   c_flag = (value) & C_FLAG; \
}

/* Combine flags into flag register */
#define  COMBINE_FLAGS() \
( \
   (n_flag & N_FLAG) \
   | (v_flag ? V_FLAG : 0) \
   | R_FLAG \
   | (d_flag ? D_FLAG : 0) \
   | i_flag \
   | (z_flag ? 0 : Z_FLAG) \
   | c_flag \
)

/* Set N and Z flags based on given value */
#define  SET_NZ_FLAGS(value)     n_flag = z_flag = (value);

/* For BCC, BCS, BEQ, BMI, BNE, BPL, BVC, BVS */
#define RELATIVE_BRANCH(condition) \
{ \
   if (condition) \
   { \
      IMMEDIATE_BYTE(btemp); \
      if (((int8_t) btemp + (PC & 0x00ff)) & 0x100) \
         ADD_CYCLES(1); \
      ADD_CYCLES(3); \
      PC += (int8_t) btemp; \
   } \
   else \
   { \
      PC++; \
      ADD_CYCLES(2); \
   } \
}

#define JUMP(address) \
{ \
   PC = bank_readword((address)); \
}

/*
** Interrupt macros
*/
#define INT_PROC(vector) \
{ \
   PUSH(PC >> 8); \
   PUSH(PC & 0xff); \
   PUSH(COMBINE_FLAGS()); \
   i_flag = I_FLAG; \
   JUMP(vector); \
}

#define NMI_PROC() \
{ \
   INT_PROC(NMI_VECTOR); \
}

#define IRQ_PROC() \
{ \
   INT_PROC(IRQ_VECTOR); \
}

/*
** Instruction macros
*/

#define ADC(cycles, read_func) \
{ \
   read_func(data); \
   temp = A + data + c_flag; \
   c_flag = (temp >> 8) & 1; \
   v_flag = ((~(A ^ data)) & (A ^ temp) & 0x80); \
   A = (uint8_t) temp; \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(cycles); \
}

/* undocumented */
#define ANC(cycles, read_func) \
{ \
   read_func(data); \
   A &= data; \
   SET_NZ_FLAGS(A); \
   c_flag = (n_flag & N_FLAG) >> 7; \
   ADD_CYCLES(cycles); \
}

#define AND(cycles, read_func) \
{ \
   read_func(data); \
   A &= data; \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(cycles); \
}

/* undocumented */
#define ANE(cycles, read_func) \
{ \
   read_func(data); \
   A = (A | 0xee) & X & data; \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(cycles); \
}

#define ARR(cycles, read_func) \
{ \
   read_func(data); \
   data &= A; \
   A = (data >> 1) | (c_flag << 7); \
   SET_NZ_FLAGS(A); \
   c_flag = (A & 0x40) >> 6; \
   v_flag = ((A >> 6) ^ (A >> 5)) & 1; \
   ADD_CYCLES(cycles); \
}

#define ASL(cycles, read_func, write_func, addr) \
{ \
   read_func(addr, data); \
   RMW_STEP(write_func, addr, data); \
   c_flag = data >> 7; \
   data <<= 1; \
   write_func(addr, data); \
   SET_NZ_FLAGS(data); \
   ADD_CYCLES(cycles); \
}

#define ASL_A() \
{ \
   c_flag = A >> 7; \
   A <<= 1; \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(2); \
}

/* undocumented */
#define ASR(cycles, read_func) \
{ \
   read_func(data); \
   data &= A; \
   c_flag = data & 1; \
   A = data >> 1; \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(cycles); \
}

#define BCC() \
{ \
   RELATIVE_BRANCH(0 == c_flag); \
}

#define BCS() \
{ \
   RELATIVE_BRANCH(0 != c_flag); \
}

#define BEQ() \
{ \
   RELATIVE_BRANCH(0 == z_flag); \
}

/* bit 7/6 of data move into N/V flags */
#define BIT(cycles, read_func) \
{ \
   read_func(data); \
   n_flag = data; \
   v_flag = data & V_FLAG; \
   z_flag = data & A; \
   ADD_CYCLES(cycles); \
}

#define BMI() \
{ \
   RELATIVE_BRANCH(n_flag & N_FLAG); \
}

#define BNE() \
{ \
   RELATIVE_BRANCH(0 != z_flag); \
}

#define BPL() \
{ \
   RELATIVE_BRANCH(0 == (n_flag & N_FLAG)); \
}

/* Software interrupt type thang */
#define BRK() \
{ \
   PC++; \
   PUSH(PC >> 8); \
   PUSH(PC & 0xff); \
   PUSH(COMBINE_FLAGS() | B_FLAG); \
   i_flag = I_FLAG; \
   JUMP(IRQ_VECTOR); \
   ADD_CYCLES(7); \
}

#define BVC() \
{ \
   RELATIVE_BRANCH(0 == v_flag); \
}

#define BVS() \
{ \
   RELATIVE_BRANCH(0 != v_flag); \
}

#define CLC() \
{ \
   c_flag = 0; \
   ADD_CYCLES(2); \
}

#define CLD() \
{ \
   d_flag = 0; \
   ADD_CYCLES(2); \
}

#define CLI() \
{ \
   ADD_CYCLES(2); \
   i_flag = 0; \
   if (cpu.int_pending && cpu.remaining_cycles > 0) \
   { \
      cpu.int_pending = false; \
      IRQ_PROC(); \
      ADD_CYCLES(INT_CYCLES); \
   } \
}

#define CLV() \
{ \
   v_flag = 0; \
   ADD_CYCLES(2); \
}

/* C is clear when data > A */ 
#define _COMPARE(reg, value) \
{ \
   temp = (reg) - (value); \
   c_flag = ((temp & 0x100) >> 8) ^ 1; \
   SET_NZ_FLAGS((uint8_t) temp); \
}

#define CMP(cycles, read_func) \
{ \
   read_func(data); \
   _COMPARE(A, data); \
   ADD_CYCLES(cycles); \
}

#define CPX(cycles, read_func) \
{ \
   read_func(data); \
   _COMPARE(X, data); \
   ADD_CYCLES(cycles); \
}

#define CPY(cycles, read_func) \
{ \
   read_func(data); \
   _COMPARE(Y, data); \
   ADD_CYCLES(cycles); \
}

/* undocumented */
#define DCP(cycles, read_func, write_func, addr) \
{ \
   read_func(addr, data); \
   RMW_STEP(write_func, addr, data); \
   data--; \
   write_func(addr, data); \
   CMP(cycles, DUMMY_READ); \
}

#define DEC(cycles, read_func, write_func, addr) \
{ \
   read_func(addr, data); \
   RMW_STEP(write_func, addr, data); \
   data--; \
   write_func(addr, data); \
   SET_NZ_FLAGS(data); \
   ADD_CYCLES(cycles); \
}

#define DEX() \
{ \
   X--; \
   SET_NZ_FLAGS(X); \
   ADD_CYCLES(2); \
}

#define DEY() \
{ \
   Y--; \
   SET_NZ_FLAGS(Y); \
   ADD_CYCLES(2); \
}

/* undocumented (double-NOP) */
#define DOP(cycles) \
{ \
   PC++; \
   ADD_CYCLES(cycles); \
}

#define EOR(cycles, read_func) \
{ \
   read_func(data); \
   A ^= data; \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(cycles); \
}

#define INC(cycles, read_func, write_func, addr) \
{ \
   read_func(addr, data); \
   RMW_STEP(write_func, addr, data); \
   data++; \
   write_func(addr, data); \
   SET_NZ_FLAGS(data); \
   ADD_CYCLES(cycles); \
}

#define INX() \
{ \
   X++; \
   SET_NZ_FLAGS(X); \
   ADD_CYCLES(2); \
}

#define INY() \
{ \
   Y++; \
   SET_NZ_FLAGS(Y); \
   ADD_CYCLES(2); \
}

/* undocumented */
#define ISB(cycles, read_func, write_func, addr) \
{ \
   read_func(addr, data); \
   RMW_STEP(write_func, addr, data); \
   data++; \
   write_func(addr, data); \
   SBC(cycles, DUMMY_READ); \
}

#define JAM() \
{ \
   if (cpu.jam_callback) \
   { \
      STORE_LOCAL_REGS(); \
      cpu.jam_callback(&cpu); \
   } \
   else \
   { \
      PC--; \
      cpu.jammed = true; \
      cpu.int_pending = false; \
      ADD_CYCLES(2); \
   } \
}

#define JMP_INDIRECT() \
{ \
   temp = bank_readword(PC); \
   /* bug in crossing page boundaries */ \
   if (0xff == (temp & 0xff)) \
      PC = (bank_readbyte(temp & 0xff00) << 8) | bank_readbyte(temp); \
   else \
      JUMP(temp); \
   ADD_CYCLES(5); \
}

#define JMP_ABSOLUTE() \
{ \
   JUMP(PC); \
   ADD_CYCLES(3); \
}

#define JSR() \
{ \
   PC++; \
   PUSH(PC >> 8); \
   PUSH(PC & 0xff); \
   JUMP(PC - 1); \
   ADD_CYCLES(6); \
}

/* undocumented */
#define LAS(cycles, read_func) \
{ \
   read_func(data); \
   A = X = S = (S & data); \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(cycles); \
}

/* undocumented */
#define LAX(cycles, read_func) \
{ \
   read_func(A); \
   X = A; \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(cycles); \
}

#define LDA(cycles, read_func) \
{ \
   read_func(A); \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(cycles); \
}

#define LDX(cycles, read_func) \
{ \
   read_func(X); \
   SET_NZ_FLAGS(X);\
   ADD_CYCLES(cycles); \
}

#define LDY(cycles, read_func) \
{ \
   read_func(Y); \
   SET_NZ_FLAGS(Y);\
   ADD_CYCLES(cycles); \
}

#define LSR(cycles, read_func, write_func, addr) \
{ \
   read_func(addr, data); \
   RMW_STEP(write_func, addr, data); \
   c_flag = data & 1; \
   data >>= 1; \
   write_func(addr, data); \
   SET_NZ_FLAGS(data); \
   ADD_CYCLES(cycles); \
}

#define LSR_A() \
{ \
   c_flag = A & 1; \
   A >>= 1; \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(2); \
}

/* undocumented */
#define LXA(cycles, read_func) \
{ \
   read_func(data); \
   A = X = ((A | 0xee) & data); \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(cycles); \
}

#define NOP() \
{ \
   ADD_CYCLES(2); \
}

#define ORA(cycles, read_func) \
{ \
   read_func(data); \
   A |= data; \
   SET_NZ_FLAGS(A);\
   ADD_CYCLES(cycles); \
}

#define PHA() \
{ \
   PUSH(A); \
   ADD_CYCLES(3); \
}

#define PHP() \
{ \
   /* B flag is pushed on stack as well */ \
   PUSH(COMBINE_FLAGS() | B_FLAG); \
   ADD_CYCLES(3); \
}

#define PLA() \
{ \
   A = PULL(); \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(4); \
}

#define PLP() \
{ \
   btemp = PULL(); \
   SCATTER_FLAGS(btemp); \
   ADD_CYCLES(4); \
}

/* undocumented */
#define RLA(cycles, read_func, write_func, addr) \
{ \
   read_func(addr, data); \
   RMW_STEP(write_func, addr, data); \
   btemp = c_flag; \
   c_flag = data >> 7; \
   data = (data << 1) | btemp; \
   write_func(addr, data); \
   A &= data; \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(cycles); \
}

/* 9-bit rotation (carry flag used for rollover) */
#define ROL(cycles, read_func, write_func, addr) \
{ \
   read_func(addr, data); \
   RMW_STEP(write_func, addr, data); \
   btemp = c_flag; \
   c_flag = data >> 7; \
   data = (data << 1) | btemp; \
   write_func(addr, data); \
   SET_NZ_FLAGS(data); \
   ADD_CYCLES(cycles); \
}

#define ROL_A() \
{ \
   btemp = c_flag; \
   c_flag = A >> 7; \
   A = (A << 1) | btemp; \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(2); \
}

#define ROR(cycles, read_func, write_func, addr) \
{ \
   read_func(addr, data); \
   RMW_STEP(write_func, addr, data); \
   btemp = c_flag << 7; \
   c_flag = data & 1; \
   data = (data >> 1) | btemp; \
   write_func(addr, data); \
   SET_NZ_FLAGS(data); \
   ADD_CYCLES(cycles); \
}

#define ROR_A() \
{ \
   btemp = c_flag << 7; \
   c_flag = A & 1; \
   A = (A >> 1) | btemp; \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(2); \
}

/* undocumented */
#define RRA(cycles, read_func, write_func, addr) \
{ \
   read_func(addr, data); \
   RMW_STEP(write_func, addr, data); \
   btemp = c_flag << 7; \
   c_flag = data & 1; \
   data = (data >> 1) | btemp; \
   write_func(addr, data); \
   ADC(cycles, DUMMY_READ); \
}

#define RTI() \
{ \
   btemp = PULL(); \
   SCATTER_FLAGS(btemp); \
   PC = PULL(); \
   PC |= PULL() << 8; \
   ADD_CYCLES(6); \
   if (0 == i_flag && cpu.int_pending && cpu.remaining_cycles > 0) \
   { \
      cpu.int_pending = false; \
      IRQ_PROC(); \
      ADD_CYCLES(INT_CYCLES); \
   } \
}

#define RTS() \
{ \
   PC = PULL(); \
   PC = (PC | (PULL() << 8)) + 1; \
   ADD_CYCLES(6); \
}

/* undocumented */
#define SAX(cycles, addr_func, write_func, addr) \
{ \
   addr_func(addr); \
   data = A & X; \
   write_func(addr, data); \
   ADD_CYCLES(cycles); \
}

#define SBC(cycles, read_func) \
{ \
   read_func(data); \
   temp = A - data - (c_flag ^ 1); \
   v_flag = (A ^ data) & (A ^ temp) & 0x80; \
   c_flag = ((temp >> 8) & 1) ^ 1; \
   A = (uint8_t) temp; \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(cycles); \
}

/* undocumented */
#define SBX(cycles, read_func) \
{ \
   read_func(data); \
   temp = (A & X) - data; \
   c_flag = ((temp >> 8) & 1) ^ 1; \
   X = temp & 0xff; \
   SET_NZ_FLAGS(X); \
   ADD_CYCLES(cycles); \
}

#define SEC() \
{ \
   c_flag = 1; \
   ADD_CYCLES(2); \
}

#define SED() \
{ \
   d_flag = 1; \
   ADD_CYCLES(2); \
}

#define SEI() \
{ \
   i_flag = I_FLAG; \
   ADD_CYCLES(2); \
}

/* undocumented */
#define SHA(cycles, addr_func, write_func, addr) \
{ \
   addr_func(addr); \
   data = A & X & ((uint8_t) ((addr >> 8) + 1)); \
   write_func(addr, data); \
   ADD_CYCLES(cycles); \
}

/* undocumented */
#define SHS(cycles, addr_func, write_func, addr) \
{ \
   addr_func(addr); \
   S = A & X; \
   data = S & ((uint8_t) ((addr >> 8) + 1)); \
   write_func(addr, data); \
   ADD_CYCLES(cycles); \
}

/* undocumented */
#define SHX(cycles, addr_func, write_func, addr) \
{ \
   addr_func(addr); \
   data = X & ((uint8_t) ((addr >> 8) + 1)); \
   write_func(addr, data); \
   ADD_CYCLES(cycles); \
}

/* undocumented */
#define SHY(cycles, addr_func, write_func, addr) \
{ \
   addr_func(addr); \
   data = Y & ((uint8_t) ((addr >> 8 ) + 1)); \
   write_func(addr, data); \
   ADD_CYCLES(cycles); \
}

/* undocumented */
#define SLO(cycles, read_func, write_func, addr) \
{ \
   read_func(addr, data); \
   RMW_STEP(write_func, addr, data); \
   c_flag = data >> 7; \
   data <<= 1; \
   write_func(addr, data); \
   A |= data; \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(cycles); \
}

/* undocumented */
#define SRE(cycles, read_func, write_func, addr) \
{ \
   read_func(addr, data); \
   RMW_STEP(write_func, addr, data); \
   c_flag = data & 1; \
   data >>= 1; \
   write_func(addr, data); \
   A ^= data; \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(cycles); \
}

#define STA(cycles, addr_func, write_func, addr) \
{ \
   addr_func(addr); \
   write_func(addr, A); \
   ADD_CYCLES(cycles); \
}

#define STX(cycles, addr_func, write_func, addr) \
{ \
   addr_func(addr); \
   write_func(addr, X); \
   ADD_CYCLES(cycles); \
}

#define STY(cycles, addr_func, write_func, addr) \
{ \
   addr_func(addr); \
   write_func(addr, Y); \
   ADD_CYCLES(cycles); \
}

#define TAX() \
{ \
   X = A; \
   SET_NZ_FLAGS(X);\
   ADD_CYCLES(2); \
}

#define TAY() \
{ \
   Y = A; \
   SET_NZ_FLAGS(Y);\
   ADD_CYCLES(2); \
}

/* undocumented (triple-NOP) */
#define TOP() \
{ \
   PC += 2; \
   ADD_CYCLES(4); \
}

#define TSX() \
{ \
   X = S; \
   SET_NZ_FLAGS(X);\
   ADD_CYCLES(2); \
}

#define TXA() \
{ \
   A = X; \
   SET_NZ_FLAGS(A);\
   ADD_CYCLES(2); \
}

#define TXS() \
{ \
   S = X; \
   ADD_CYCLES(2); \
}

#define TYA() \
{ \
   A = Y; \
   SET_NZ_FLAGS(A); \
   ADD_CYCLES(2); \
}

/* internal CPU context */
static nes6502_context cpu;

/*
** Zero-page helper macros
*/

#define  ZP_READBYTE(addr)          cpu.mem_page[0][(addr)]
#define  ZP_WRITEBYTE(addr, value)  cpu.mem_page[0][(addr)] = (uint8_t) (value)

/* NOTE: bank_readbyte、bank_writebyte、bank_readword
** these functions should only read/write memory when it's address > 0x6000
*/
INLINE uint8_t bank_readbyte(uint_fast16_t address)
{
   return cpu.mem_page[address / NES6502_BANKSIZE][address % NES6502_BANKSIZE];
}

INLINE void bank_writebyte(uint_fast16_t address, uint8_t value)
{
   cpu.mem_page[address / NES6502_BANKSIZE][address % NES6502_BANKSIZE] = value;
}

#ifndef HOST_BIG_ENDIAN

/* NOTE: following two functions will fail on architectures
** which do not support unaligned accesses
*/
INLINE uint_fast16_t zp_readword(uint8_t address)
{
   return (uint_fast16_t) (*(uint16_t *)(cpu.mem_page[0] + address));
}

INLINE uint_fast16_t bank_readword(uint_fast16_t address)
{
   /* technically, this should fail if the address is $xFFF, but
   ** any code that does this would be suspect anyway, as it would
   ** be fetching a word across page boundaries, which only would
   ** make sense if the banks were physically consecutive.
   */
   return (uint_fast16_t) (*(uint16_t *)(cpu.mem_page[address / NES6502_BANKSIZE] + (address % NES6502_BANKSIZE)));
}

#else /* HOST_BIG_ENDIAN */

INLINE uint_fast16_t zp_readword(uint8_t address)
{
    uint_fast16_t x = (uint_fast16_t) *(uint16_t *)(cpu.mem_page[0] + address);
   return (x << 8) | (x >> 8);
}

INLINE uint_fast16_t bank_readword(uint_fast16_t address)
{
    uint_fast16_t x = (uint_fast16_t) *(uint16_t *)(cpu.mem_page[address / NES6502_BANKSIZE] + (address % NES6502_BANKSIZE));
   return (x << 8) | (x >> 8);
}

#endif /* HOST_BIG_ENDIAN */

/* read a byte of 6502 memory */
static uint8_t mem_readbyte(uint_fast16_t address)
{
   if (address < 0x2000)
   {
      /* RAM */
      return cpu.mem_page[0][address & 0x7ff];
   }
   else if (address >= 0x6000)
   {
      /* always paged memory */
      return bank_readbyte(address);
   }
   /* check memory range handlers */
   else
   {
      nes6502_memread *mr;

      for (int i = 0; i < cpu.read_num; ++i)
      {
         mr = &cpu.read_handler[i];
         if (address >= mr->min_range && address <= mr->max_range)
            return mr->read_func(mr->userdata, address - mr->min_range);
      }
      // TODO: warning this situation!
      return 0x00;
   }
}

/* write a byte of data to 6502 memory */
static void mem_writebyte(uint_fast16_t address, uint8_t value)
{
   /* RAM */
   if (address < 0x2000)
   {
      cpu.mem_page[0][address & 0x7ff] = value;
      return;
   }
   /* check memory range handlers */
   else
   {
      nes6502_memwrite *mw;

      for (int i = 0; i < cpu.write_num; ++i)
      {
         mw = &cpu.write_handler[i];
         if (address >= mw->min_range && address <= mw->max_range)
         {
            return mw->write_func(mw->userdata, address - mw->min_range, value);
         }
      }
   }
   if (address >= 0x6000) {
       bank_writebyte(address, value);
   }
   // TODO: warning this situation!
   // where to write?
}

/* set the current context */
void nes6502_setcontext(nes6502_context *context)
{
   if (NULL == context)
      return;

   cpu = *context;
}

/* get the current context */
void nes6502_getcontext(nes6502_context *context)
{
   if (NULL == context)
      return;

   *context = cpu;
}

uint8_t nes6502_getbyte(uint16_t address) __attribute__ ((weak, alias ("mem_readbyte"))); // @suppress("Unused function declaration")

/* get number of elapsed cycles */
long nes6502_getcycles(bool reset_flag)
{
   long cycles = cpu.total_cycles;

   if (reset_flag)
      cpu.total_cycles = 0;

   return cycles;
}


#ifdef NES6502_JUMPTABLE

#define  OPCODE_BEGIN(xx)  op##xx:
#define  OPCODE_END        if (cpu.remaining_cycles <= 0) \
                              goto end_execute; \
                           DISASM_INSTRUCTION(PC, COMBINE_FLAGS(), A, X, Y, S); \
                           goto *opcode_table[bank_readbyte(PC++)];

#else /* !NES6502_JUMPTABLE */

#define  OPCODE_BEGIN(xx)  case 0x##xx:
#define  OPCODE_END        break;

#endif /* !NES6502_JUMPTABLE */


/* Execute instructions until count expires
**
** Returns the number of cycles *actually* executed, which will be
** anywhere from zero to timeslice_cycles + 6
*/
long nes6502_execute(long timeslice_cycles)
{
   long old_cycles = cpu.total_cycles;

   uint_fast16_t temp, addr; /* for macros */
   uint8_t btemp, baddr; /* for macros */
   uint8_t data;

   /* flags */
   uint8_t n_flag, v_flag;
   uint8_t d_flag, i_flag, z_flag, c_flag;

   /* local copies of regs */
   uint_fast16_t PC;
   uint8_t A, X, Y, S;

#ifdef NES6502_JUMPTABLE
   
   static const void * const opcode_table[256] =
   {
      &&op00, &&op01, &&op02, &&op03, &&op04, &&op05, &&op06, &&op07,
      &&op08, &&op09, &&op0a, &&op0b, &&op0c, &&op0d, &&op0e, &&op0f,
      &&op10, &&op11, &&op12, &&op13, &&op14, &&op15, &&op16, &&op17,
      &&op18, &&op19, &&op1a, &&op1b, &&op1c, &&op1d, &&op1e, &&op1f,
      &&op20, &&op21, &&op22, &&op23, &&op24, &&op25, &&op26, &&op27,
      &&op28, &&op29, &&op2a, &&op2b, &&op2c, &&op2d, &&op2e, &&op2f,
      &&op30, &&op31, &&op32, &&op33, &&op34, &&op35, &&op36, &&op37,
      &&op38, &&op39, &&op3a, &&op3b, &&op3c, &&op3d, &&op3e, &&op3f,
      &&op40, &&op41, &&op42, &&op43, &&op44, &&op45, &&op46, &&op47,
      &&op48, &&op49, &&op4a, &&op4b, &&op4c, &&op4d, &&op4e, &&op4f,
      &&op50, &&op51, &&op52, &&op53, &&op54, &&op55, &&op56, &&op57,
      &&op58, &&op59, &&op5a, &&op5b, &&op5c, &&op5d, &&op5e, &&op5f,
      &&op60, &&op61, &&op62, &&op63, &&op64, &&op65, &&op66, &&op67,
      &&op68, &&op69, &&op6a, &&op6b, &&op6c, &&op6d, &&op6e, &&op6f,
      &&op70, &&op71, &&op72, &&op73, &&op74, &&op75, &&op76, &&op77,
      &&op78, &&op79, &&op7a, &&op7b, &&op7c, &&op7d, &&op7e, &&op7f,
      &&op80, &&op81, &&op82, &&op83, &&op84, &&op85, &&op86, &&op87,
      &&op88, &&op89, &&op8a, &&op8b, &&op8c, &&op8d, &&op8e, &&op8f,
      &&op90, &&op91, &&op92, &&op93, &&op94, &&op95, &&op96, &&op97,
      &&op98, &&op99, &&op9a, &&op9b, &&op9c, &&op9d, &&op9e, &&op9f,
      &&opa0, &&opa1, &&opa2, &&opa3, &&opa4, &&opa5, &&opa6, &&opa7,
      &&opa8, &&opa9, &&opaa, &&opab, &&opac, &&opad, &&opae, &&opaf,
      &&opb0, &&opb1, &&opb2, &&opb3, &&opb4, &&opb5, &&opb6, &&opb7,
      &&opb8, &&opb9, &&opba, &&opbb, &&opbc, &&opbd, &&opbe, &&opbf,
      &&opc0, &&opc1, &&opc2, &&opc3, &&opc4, &&opc5, &&opc6, &&opc7,
      &&opc8, &&opc9, &&opca, &&opcb, &&opcc, &&opcd, &&opce, &&opcf,
      &&opd0, &&opd1, &&opd2, &&opd3, &&opd4, &&opd5, &&opd6, &&opd7,
      &&opd8, &&opd9, &&opda, &&opdb, &&opdc, &&opdd, &&opde, &&opdf,
      &&ope0, &&ope1, &&ope2, &&ope3, &&ope4, &&ope5, &&ope6, &&ope7,
      &&ope8, &&ope9, &&opea, &&opeb, &&opec, &&oped, &&opee, &&opef,
      &&opf0, &&opf1, &&opf2, &&opf3, &&opf4, &&opf5, &&opf6, &&opf7,
      &&opf8, &&opf9, &&opfa, &&opfb, &&opfc, &&opfd, &&opfe, &&opff
   };

#endif /* NES6502_JUMPTABLE */

   cpu.remaining_cycles = timeslice_cycles;

   GET_GLOBAL_REGS();

   /* check for DMA cycle burning */
   if (cpu.burn_cycles && cpu.remaining_cycles > 0)
   {
      int burn_for = MIN(cpu.remaining_cycles, cpu.burn_cycles);
      ADD_CYCLES(burn_for);
      cpu.burn_cycles -= burn_for;
   }

   if (0 == i_flag && cpu.int_pending && cpu.remaining_cycles > 0)
   {
      cpu.int_pending = false;
      IRQ_PROC();
      ADD_CYCLES(INT_CYCLES);
   }

#ifdef NES6502_JUMPTABLE
   /* fetch first instruction */
   OPCODE_END

#else /* !NES6502_JUMPTABLE */

   /* Continue until we run out of cycles */
   while (cpu.remaining_cycles > 0)
   {
#ifdef NES6502_DISASM
      printf(nes6502_disasm(PC, COMBINE_FLAGS(), A, X, Y, S));
#endif /* NES6502_DISASM */

      /* Fetch and execute instruction */
      switch (bank_readbyte(PC++))
      {
#endif /* !NES6502_JUMPTABLE */

      OPCODE_BEGIN(00)  /* BRK */
         BRK();
      OPCODE_END

      OPCODE_BEGIN(01)  /* ORA ($nn,X) */
         ORA(6, INDIR_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(02)  /* JAM */
      OPCODE_BEGIN(12)  /* JAM */
      OPCODE_BEGIN(22)  /* JAM */
      OPCODE_BEGIN(32)  /* JAM */
      OPCODE_BEGIN(42)  /* JAM */
      OPCODE_BEGIN(52)  /* JAM */
      OPCODE_BEGIN(62)  /* JAM */
      OPCODE_BEGIN(72)  /* JAM */
      OPCODE_BEGIN(92)  /* JAM */
      OPCODE_BEGIN(b2)  /* JAM */
      OPCODE_BEGIN(d2)  /* JAM */
      OPCODE_BEGIN(f2)  /* JAM */
         JAM();
         /* kill the CPU */
         cpu.remaining_cycles = 0;
      OPCODE_END

      OPCODE_BEGIN(03)  /* SLO ($nn,X) */
         SLO(8, INDIR_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(04)  /* NOP $nn */
      OPCODE_BEGIN(44)  /* NOP $nn */
      OPCODE_BEGIN(64)  /* NOP $nn */
         DOP(3);
      OPCODE_END

      OPCODE_BEGIN(05)  /* ORA $nn */
         ORA(3, ZERO_PAGE_BYTE); 
      OPCODE_END

      OPCODE_BEGIN(06)  /* ASL $nn */
         ASL(5, ZERO_PAGE, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(07)  /* SLO $nn */
         SLO(5, ZERO_PAGE, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(08)  /* PHP */
         PHP(); 
      OPCODE_END

      OPCODE_BEGIN(09)  /* ORA #$nn */
         ORA(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(0a)  /* ASL A */
         ASL_A();
      OPCODE_END

      OPCODE_BEGIN(0b)  /* ANC #$nn */
         ANC(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(0c)  /* NOP $nnnn */
         TOP(); 
      OPCODE_END

      OPCODE_BEGIN(0d)  /* ORA $nnnn */
         ORA(4, ABSOLUTE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(0e)  /* ASL $nnnn */
         ASL(6, ABSOLUTE, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(0f)  /* SLO $nnnn */
         SLO(6, ABSOLUTE, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(10)  /* BPL $nnnn */
         BPL();
      OPCODE_END

      OPCODE_BEGIN(11)  /* ORA ($nn),Y */
         ORA(5, INDIR_Y_BYTE_READ);
      OPCODE_END
      
      OPCODE_BEGIN(13)  /* SLO ($nn),Y */
         SLO(8, INDIR_Y, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(14)  /* NOP $nn,X */
      OPCODE_BEGIN(34)  /* NOP */
      OPCODE_BEGIN(54)  /* NOP $nn,X */
      OPCODE_BEGIN(74)  /* NOP $nn,X */
      OPCODE_BEGIN(d4)  /* NOP $nn,X */
      OPCODE_BEGIN(f4)  /* NOP ($nn,X) */
         DOP(4);
      OPCODE_END

      OPCODE_BEGIN(15)  /* ORA $nn,X */
         ORA(4, ZP_IND_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(16)  /* ASL $nn,X */
         ASL(6, ZP_IND_X, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(17)  /* SLO $nn,X */
         SLO(6, ZP_IND_X, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(18)  /* CLC */
         CLC();
      OPCODE_END

      OPCODE_BEGIN(19)  /* ORA $nnnn,Y */
         ORA(4, ABS_IND_Y_BYTE_READ);
      OPCODE_END
      
      OPCODE_BEGIN(1a)  /* NOP */
      OPCODE_BEGIN(3a)  /* NOP */
      OPCODE_BEGIN(5a)  /* NOP */
      OPCODE_BEGIN(7a)  /* NOP */
      OPCODE_BEGIN(da)  /* NOP */
      OPCODE_BEGIN(fa)  /* NOP */
         NOP();
      OPCODE_END

      OPCODE_BEGIN(1b)  /* SLO $nnnn,Y */
         SLO(7, ABS_IND_Y, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(1c)  /* NOP $nnnn,X */
      OPCODE_BEGIN(3c)  /* NOP $nnnn,X */
      OPCODE_BEGIN(5c)  /* NOP $nnnn,X */
      OPCODE_BEGIN(7c)  /* NOP $nnnn,X */
      OPCODE_BEGIN(dc)  /* NOP $nnnn,X */
      OPCODE_BEGIN(fc)  /* NOP $nnnn,X */
         TOP();
      OPCODE_END

      OPCODE_BEGIN(1d)  /* ORA $nnnn,X */
         ORA(4, ABS_IND_X_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(1e)  /* ASL $nnnn,X */
         ASL(7, ABS_IND_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(1f)  /* SLO $nnnn,X */
         SLO(7, ABS_IND_X, mem_writebyte, addr);
      OPCODE_END
      
      OPCODE_BEGIN(20)  /* JSR $nnnn */
         JSR();
      OPCODE_END

      OPCODE_BEGIN(21)  /* AND ($nn,X) */
         AND(6, INDIR_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(23)  /* RLA ($nn,X) */
         RLA(8, INDIR_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(24)  /* BIT $nn */
         BIT(3, ZERO_PAGE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(25)  /* AND $nn */
         AND(3, ZERO_PAGE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(26)  /* ROL $nn */
         ROL(5, ZERO_PAGE, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(27)  /* RLA $nn */
         RLA(5, ZERO_PAGE, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(28)  /* PLP */
         PLP();
      OPCODE_END

      OPCODE_BEGIN(29)  /* AND #$nn */
         AND(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(2a)  /* ROL A */
         ROL_A();
      OPCODE_END

      OPCODE_BEGIN(2b)  /* ANC #$nn */
         ANC(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(2c)  /* BIT $nnnn */
         BIT(4, ABSOLUTE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(2d)  /* AND $nnnn */
         AND(4, ABSOLUTE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(2e)  /* ROL $nnnn */
         ROL(6, ABSOLUTE, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(2f)  /* RLA $nnnn */
         RLA(6, ABSOLUTE, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(30)  /* BMI $nnnn */
         BMI();
      OPCODE_END

      OPCODE_BEGIN(31)  /* AND ($nn),Y */
         AND(5, INDIR_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(33)  /* RLA ($nn),Y */
         RLA(8, INDIR_Y, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(35)  /* AND $nn,X */
         AND(4, ZP_IND_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(36)  /* ROL $nn,X */
         ROL(6, ZP_IND_X, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(37)  /* RLA $nn,X */
         RLA(6, ZP_IND_X, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(38)  /* SEC */
         SEC();
      OPCODE_END

      OPCODE_BEGIN(39)  /* AND $nnnn,Y */
         AND(4, ABS_IND_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(3b)  /* RLA $nnnn,Y */
         RLA(7, ABS_IND_Y, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(3d)  /* AND $nnnn,X */
         AND(4, ABS_IND_X_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(3e)  /* ROL $nnnn,X */
         ROL(7, ABS_IND_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(3f)  /* RLA $nnnn,X */
         RLA(7, ABS_IND_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(40)  /* RTI */
         RTI();
      OPCODE_END

      OPCODE_BEGIN(41)  /* EOR ($nn,X) */
         EOR(6, INDIR_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(43)  /* SRE ($nn,X) */
         SRE(8, INDIR_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(45)  /* EOR $nn */
         EOR(3, ZERO_PAGE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(46)  /* LSR $nn */
         LSR(5, ZERO_PAGE, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(47)  /* SRE $nn */
         SRE(5, ZERO_PAGE, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(48)  /* PHA */
         PHA();
      OPCODE_END

      OPCODE_BEGIN(49)  /* EOR #$nn */
         EOR(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(4a)  /* LSR A */
         LSR_A();
      OPCODE_END

      OPCODE_BEGIN(4b)  /* ASR #$nn */
         ASR(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(4c)  /* JMP $nnnn */
         JMP_ABSOLUTE();
      OPCODE_END

      OPCODE_BEGIN(4d)  /* EOR $nnnn */
         EOR(4, ABSOLUTE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(4e)  /* LSR $nnnn */
         LSR(6, ABSOLUTE, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(4f)  /* SRE $nnnn */
         SRE(6, ABSOLUTE, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(50)  /* BVC $nnnn */
         BVC();
      OPCODE_END

      OPCODE_BEGIN(51)  /* EOR ($nn),Y */
         EOR(5, INDIR_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(53)  /* SRE ($nn),Y */
         SRE(8, INDIR_Y, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(55)  /* EOR $nn,X */
         EOR(4, ZP_IND_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(56)  /* LSR $nn,X */
         LSR(6, ZP_IND_X, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(57)  /* SRE $nn,X */
         SRE(6, ZP_IND_X, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(58)  /* CLI */
         CLI();
      OPCODE_END

      OPCODE_BEGIN(59)  /* EOR $nnnn,Y */
         EOR(4, ABS_IND_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(5b)  /* SRE $nnnn,Y */
         SRE(7, ABS_IND_Y, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(5d)  /* EOR $nnnn,X */
         EOR(4, ABS_IND_X_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(5e)  /* LSR $nnnn,X */
         LSR(7, ABS_IND_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(5f)  /* SRE $nnnn,X */
         SRE(7, ABS_IND_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(60)  /* RTS */
         RTS();
      OPCODE_END

      OPCODE_BEGIN(61)  /* ADC ($nn,X) */
         ADC(6, INDIR_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(63)  /* RRA ($nn,X) */
         RRA(8, INDIR_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(65)  /* ADC $nn */
         ADC(3, ZERO_PAGE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(66)  /* ROR $nn */
         ROR(5, ZERO_PAGE, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(67)  /* RRA $nn */
         RRA(5, ZERO_PAGE, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(68)  /* PLA */
         PLA();
      OPCODE_END

      OPCODE_BEGIN(69)  /* ADC #$nn */
         ADC(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(6a)  /* ROR A */
         ROR_A();
      OPCODE_END

      OPCODE_BEGIN(6b)  /* ARR #$nn */
         ARR(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(6c)  /* JMP ($nnnn) */
         JMP_INDIRECT();
      OPCODE_END

      OPCODE_BEGIN(6d)  /* ADC $nnnn */
         ADC(4, ABSOLUTE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(6e)  /* ROR $nnnn */
         ROR(6, ABSOLUTE, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(6f)  /* RRA $nnnn */
         RRA(6, ABSOLUTE, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(70)  /* BVS $nnnn */
         BVS();
      OPCODE_END

      OPCODE_BEGIN(71)  /* ADC ($nn),Y */
         ADC(5, INDIR_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(73)  /* RRA ($nn),Y */
         RRA(8, INDIR_Y, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(75)  /* ADC $nn,X */
         ADC(4, ZP_IND_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(76)  /* ROR $nn,X */
         ROR(6, ZP_IND_X, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(77)  /* RRA $nn,X */
         RRA(6, ZP_IND_X, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(78)  /* SEI */
         SEI();
      OPCODE_END

      OPCODE_BEGIN(79)  /* ADC $nnnn,Y */
         ADC(4, ABS_IND_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(7b)  /* RRA $nnnn,Y */
         RRA(7, ABS_IND_Y, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(7d)  /* ADC $nnnn,X */
         ADC(4, ABS_IND_X_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(7e)  /* ROR $nnnn,X */
         ROR(7, ABS_IND_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(7f)  /* RRA $nnnn,X */
         RRA(7, ABS_IND_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(80)  /* NOP #$nn */
      OPCODE_BEGIN(82)  /* NOP #$nn */
      OPCODE_BEGIN(89)  /* NOP #$nn */
      OPCODE_BEGIN(c2)  /* NOP #$nn */
      OPCODE_BEGIN(e2)  /* NOP #$nn */
         DOP(2);
      OPCODE_END

      OPCODE_BEGIN(81)  /* STA ($nn,X) */
         STA(6, INDIR_X_ADDR, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(83)  /* SAX ($nn,X) */
         SAX(6, INDIR_X_ADDR, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(84)  /* STY $nn */
         STY(3, ZERO_PAGE_ADDR, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(85)  /* STA $nn */
         STA(3, ZERO_PAGE_ADDR, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(86)  /* STX $nn */
         STX(3, ZERO_PAGE_ADDR, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(87)  /* SAX $nn */
         SAX(3, ZERO_PAGE_ADDR, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(88)  /* DEY */
         DEY();
      OPCODE_END

      OPCODE_BEGIN(8a)  /* TXA */
         TXA();
      OPCODE_END

      OPCODE_BEGIN(8b)  /* ANE #$nn */
         ANE(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(8c)  /* STY $nnnn */
         STY(4, ABSOLUTE_ADDR, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(8d)  /* STA $nnnn */
         STA(4, ABSOLUTE_ADDR, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(8e)  /* STX $nnnn */
         STX(4, ABSOLUTE_ADDR, mem_writebyte, addr);
      OPCODE_END
      
      OPCODE_BEGIN(8f)  /* SAX $nnnn */
         SAX(4, ABSOLUTE_ADDR, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(90)  /* BCC $nnnn */
         BCC();
      OPCODE_END

      OPCODE_BEGIN(91)  /* STA ($nn),Y */
         STA(6, INDIR_Y_ADDR, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(93)  /* SHA ($nn),Y */
         SHA(6, INDIR_Y_ADDR, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(94)  /* STY $nn,X */
         STY(4, ZP_IND_X_ADDR, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(95)  /* STA $nn,X */
         STA(4, ZP_IND_X_ADDR, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(96)  /* STX $nn,Y */
         STX(4, ZP_IND_Y_ADDR, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(97)  /* SAX $nn,Y */
         SAX(4, ZP_IND_Y_ADDR, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(98)  /* TYA */
         TYA();
      OPCODE_END

      OPCODE_BEGIN(99)  /* STA $nnnn,Y */
         STA(5, ABS_IND_Y_ADDR, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(9a)  /* TXS */
         TXS();
      OPCODE_END

      OPCODE_BEGIN(9b)  /* SHS $nnnn,Y */
         SHS(5, ABS_IND_Y_ADDR, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(9c)  /* SHY $nnnn,X */
         SHY(5, ABS_IND_X_ADDR, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(9d)  /* STA $nnnn,X */
         STA(5, ABS_IND_X_ADDR, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(9e)  /* SHX $nnnn,Y */
         SHX(5, ABS_IND_Y_ADDR, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(9f)  /* SHA $nnnn,Y */
         SHA(5, ABS_IND_Y_ADDR, mem_writebyte, addr);
      OPCODE_END
      
      OPCODE_BEGIN(a0)  /* LDY #$nn */
         LDY(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(a1)  /* LDA ($nn,X) */
         LDA(6, INDIR_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(a2)  /* LDX #$nn */
         LDX(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(a3)  /* LAX ($nn,X) */
         LAX(6, INDIR_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(a4)  /* LDY $nn */
         LDY(3, ZERO_PAGE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(a5)  /* LDA $nn */
         LDA(3, ZERO_PAGE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(a6)  /* LDX $nn */
         LDX(3, ZERO_PAGE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(a7)  /* LAX $nn */
         LAX(3, ZERO_PAGE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(a8)  /* TAY */
         TAY();
      OPCODE_END

      OPCODE_BEGIN(a9)  /* LDA #$nn */
         LDA(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(aa)  /* TAX */
         TAX();
      OPCODE_END

      OPCODE_BEGIN(ab)  /* LXA #$nn */
         LXA(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(ac)  /* LDY $nnnn */
         LDY(4, ABSOLUTE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(ad)  /* LDA $nnnn */
         LDA(4, ABSOLUTE_BYTE);
      OPCODE_END
      
      OPCODE_BEGIN(ae)  /* LDX $nnnn */
         LDX(4, ABSOLUTE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(af)  /* LAX $nnnn */
         LAX(4, ABSOLUTE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(b0)  /* BCS $nnnn */
         BCS();
      OPCODE_END

      OPCODE_BEGIN(b1)  /* LDA ($nn),Y */
         LDA(5, INDIR_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(b3)  /* LAX ($nn),Y */
         LAX(5, INDIR_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(b4)  /* LDY $nn,X */
         LDY(4, ZP_IND_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(b5)  /* LDA $nn,X */
         LDA(4, ZP_IND_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(b6)  /* LDX $nn,Y */
         LDX(4, ZP_IND_Y_BYTE);
      OPCODE_END

      OPCODE_BEGIN(b7)  /* LAX $nn,Y */
         LAX(4, ZP_IND_Y_BYTE);
      OPCODE_END

      OPCODE_BEGIN(b8)  /* CLV */
         CLV();
      OPCODE_END

      OPCODE_BEGIN(b9)  /* LDA $nnnn,Y */
         LDA(4, ABS_IND_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(ba)  /* TSX */
         TSX();
      OPCODE_END

      OPCODE_BEGIN(bb)  /* LAS $nnnn,Y */
         LAS(4, ABS_IND_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(bc)  /* LDY $nnnn,X */
         LDY(4, ABS_IND_X_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(bd)  /* LDA $nnnn,X */
         LDA(4, ABS_IND_X_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(be)  /* LDX $nnnn,Y */
         LDX(4, ABS_IND_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(bf)  /* LAX $nnnn,Y */
         LAX(4, ABS_IND_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(c0)  /* CPY #$nn */
         CPY(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(c1)  /* CMP ($nn,X) */
         CMP(6, INDIR_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(c3)  /* DCP ($nn,X) */
         DCP(8, INDIR_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(c4)  /* CPY $nn */
         CPY(3, ZERO_PAGE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(c5)  /* CMP $nn */
         CMP(3, ZERO_PAGE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(c6)  /* DEC $nn */
         DEC(5, ZERO_PAGE, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(c7)  /* DCP $nn */
         DCP(5, ZERO_PAGE, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(c8)  /* INY */
         INY();
      OPCODE_END

      OPCODE_BEGIN(c9)  /* CMP #$nn */
         CMP(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(ca)  /* DEX */
         DEX();
      OPCODE_END

      OPCODE_BEGIN(cb)  /* SBX #$nn */
         SBX(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(cc)  /* CPY $nnnn */
         CPY(4, ABSOLUTE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(cd)  /* CMP $nnnn */
         CMP(4, ABSOLUTE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(ce)  /* DEC $nnnn */
         DEC(6, ABSOLUTE, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(cf)  /* DCP $nnnn */
         DCP(6, ABSOLUTE, mem_writebyte, addr);
      OPCODE_END
      
      OPCODE_BEGIN(d0)  /* BNE $nnnn */
         BNE();
      OPCODE_END

      OPCODE_BEGIN(d1)  /* CMP ($nn),Y */
         CMP(5, INDIR_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(d3)  /* DCP ($nn),Y */
         DCP(8, INDIR_Y, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(d5)  /* CMP $nn,X */
         CMP(4, ZP_IND_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(d6)  /* DEC $nn,X */
         DEC(6, ZP_IND_X, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(d7)  /* DCP $nn,X */
         DCP(6, ZP_IND_X, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(d8)  /* CLD */
         CLD();
      OPCODE_END

      OPCODE_BEGIN(d9)  /* CMP $nnnn,Y */
         CMP(4, ABS_IND_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(db)  /* DCP $nnnn,Y */
         DCP(7, ABS_IND_Y, mem_writebyte, addr);
      OPCODE_END                  

      OPCODE_BEGIN(dd)  /* CMP $nnnn,X */
         CMP(4, ABS_IND_X_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(de)  /* DEC $nnnn,X */
         DEC(7, ABS_IND_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(df)  /* DCP $nnnn,X */
         DCP(7, ABS_IND_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(e0)  /* CPX #$nn */
         CPX(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(e1)  /* SBC ($nn,X) */
         SBC(6, INDIR_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(e3)  /* ISB ($nn,X) */
         ISB(8, INDIR_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(e4)  /* CPX $nn */
         CPX(3, ZERO_PAGE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(e5)  /* SBC $nn */
         SBC(3, ZERO_PAGE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(e6)  /* INC $nn */
         INC(5, ZERO_PAGE, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(e7)  /* ISB $nn */
         ISB(5, ZERO_PAGE, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(e8)  /* INX */
         INX();
      OPCODE_END

      OPCODE_BEGIN(e9)  /* SBC #$nn */
      OPCODE_BEGIN(eb)  /* USBC #$nn */
         SBC(2, IMMEDIATE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(ea)  /* NOP */
         NOP();
      OPCODE_END

      OPCODE_BEGIN(ec)  /* CPX $nnnn */
         CPX(4, ABSOLUTE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(ed)  /* SBC $nnnn */
         SBC(4, ABSOLUTE_BYTE);
      OPCODE_END

      OPCODE_BEGIN(ee)  /* INC $nnnn */
         INC(6, ABSOLUTE, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(ef)  /* ISB $nnnn */
         ISB(6, ABSOLUTE, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(f0)  /* BEQ $nnnn */
         BEQ();
      OPCODE_END

      OPCODE_BEGIN(f1)  /* SBC ($nn),Y */
         SBC(5, INDIR_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(f3)  /* ISB ($nn),Y */
         ISB(8, INDIR_Y, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(f5)  /* SBC $nn,X */
         SBC(4, ZP_IND_X_BYTE);
      OPCODE_END

      OPCODE_BEGIN(f6)  /* INC $nn,X */
         INC(6, ZP_IND_X, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(f7)  /* ISB $nn,X */
         ISB(6, ZP_IND_X, ZP_WRITEBYTE, baddr);
      OPCODE_END

      OPCODE_BEGIN(f8)  /* SED */
         SED();
      OPCODE_END

      OPCODE_BEGIN(f9)  /* SBC $nnnn,Y */
         SBC(4, ABS_IND_Y_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(fb)  /* ISB $nnnn,Y */
         ISB(7, ABS_IND_Y, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(fd)  /* SBC $nnnn,X */
         SBC(4, ABS_IND_X_BYTE_READ);
      OPCODE_END

      OPCODE_BEGIN(fe)  /* INC $nnnn,X */
         INC(7, ABS_IND_X, mem_writebyte, addr);
      OPCODE_END

      OPCODE_BEGIN(ff)  /* ISB $nnnn,X */
         ISB(7, ABS_IND_X, mem_writebyte, addr);
      OPCODE_END

#ifdef NES6502_JUMPTABLE
end_execute:

#else /* !NES6502_JUMPTABLE */
      }
   }
#endif /* !NES6502_JUMPTABLE */

   /* store local copy of regs */
   STORE_LOCAL_REGS();

   /* Return our actual amount of executed cycles */
   return (cpu.total_cycles - old_cycles);
}

/* Issue a CPU Reset */
void nes6502_reset(void)
{
   cpu.p_reg = Z_FLAG | R_FLAG | I_FLAG;     /* Reserved bit always 1 */
   cpu.int_pending = false;                  /* No pending interrupts */
   cpu.pc_reg = bank_readword(RESET_VECTOR); /* Fetch reset vector */
   cpu.burn_cycles = RESET_CYCLES;
   cpu.total_cycles = 0;
   cpu.remaining_cycles = 0;
   cpu.jammed = false;
}

/* following macro is used for below 2 functions */
#define  DECLARE_LOCAL_REGS \
   uint_fast16_t PC; \
   uint8_t A, X, Y, S; \
   uint8_t n_flag, v_flag; \
   uint8_t d_flag, i_flag, z_flag, c_flag;

/* Non-maskable interrupt */
void nes6502_nmi(void)
{
   if (false == cpu.jammed)
   {
      DECLARE_LOCAL_REGS
      GET_GLOBAL_REGS();
      NMI_PROC();
      cpu.burn_cycles += INT_CYCLES;
      STORE_LOCAL_REGS();
   }
}

/* Interrupt request */
void nes6502_irq(void)
{
   if (false == cpu.jammed)
   {
      DECLARE_LOCAL_REGS

      GET_GLOBAL_REGS();
      if (0 == i_flag)
      {
         IRQ_PROC();
         cpu.burn_cycles += INT_CYCLES;
      }
      else
      {
         cpu.int_pending = true;
      }
      STORE_LOCAL_REGS();
   }
}

/* clear any pending IRQ */
void nes6502_clearirq(void)
{
   cpu.int_pending = false;
}

/* Set burn cycle period */
void nes6502_burn(long cycles)
{
   cpu.burn_cycles += cycles;
}

/* Release our timeslice */
void nes6502_release(void)
{
   cpu.remaining_cycles = 0;
}
