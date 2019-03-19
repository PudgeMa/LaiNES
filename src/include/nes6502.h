/*
// shatbox (C) 2002 Matthew Conte (matt@conte.com)
**
**
** nes6502.h
**
** NES custom 6502 CPU
**
** $Id: $
*/

#ifndef _NES6502_H_
#define _NES6502_H_

#include <stdint.h>
#include <stdbool.h>

#define  NES6502_ADDRSPACE 0x10000  /* 64kB */
#define  NES6502_NUMBANKS  8
#define  NES6502_BANKSIZE  (NES6502_ADDRSPACE / NES6502_NUMBANKS)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct nes6502_memread_s
{
   int         min_range, max_range;
   uint8_t     (*read_func)(void *userdata, uint16_t address);
   void        *userdata;
} nes6502_memread;

typedef struct nes6502_memwrite_s
{
   int         min_range, max_range;
   void        (*write_func)(void *userdata, uint16_t address, uint8_t value);
   void        *userdata;
} nes6502_memwrite;

typedef struct nes6502_context_s
{
   /* memory page pointers
    * 0x0000 - 0x2000 RAM
    * 0x6000 - 0x8000 SRAM
    * 0x8000 - 0xffff PRG-ROM
    */
   uint8_t  *mem_page[NES6502_NUMBANKS];

    /* user callback, for when cpu hits a JAM instruction */
   void     (*jam_callback)(struct nes6502_context_s *context);

   /* memory read/write handlers for IO ports */
   nes6502_memread  *read_handler;
   nes6502_memwrite *write_handler;
   int      read_num;
   int      write_num;

   /* private data */
   long     total_cycles;
   long     burn_cycles;
   long     remaining_cycles;

   uint16_t pc_reg;
   uint8_t  a_reg, p_reg;
   uint8_t  x_reg, y_reg;
   uint8_t  s_reg;

   bool     jammed;
   bool     int_pending;
} nes6502_context;

/* Functions which govern the 6502's execution */
extern void    nes6502_reset(void);
extern long    nes6502_execute(long total_cycles);
extern void    nes6502_nmi(void);
extern void    nes6502_irq(void);
extern void    nes6502_clearirq(void);
extern uint8_t nes6502_getbyte(uint16_t address);
extern long    nes6502_getcycles(bool reset_flag);
extern void    nes6502_burn(long cycles);
extern void    nes6502_release(void);

/* Context get/set */
extern void    nes6502_setcontext(nes6502_context *cpu);
extern void    nes6502_getcontext(nes6502_context *cpu);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _NES6502_H_ */
