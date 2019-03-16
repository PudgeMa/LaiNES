#ifndef _NES6502_H_
#define _NES6502_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define  NES6502_MEMSIZE 0x0800

typedef uint8_t (*nes6502_read)(void *userdata, uint16_t offset);
typedef void  (*nes6502_write)(void *userdata, uint16_t offset, uint8_t value);

typedef struct nes6502_memread_s
{
   uint16_t       min_range, max_range;
   nes6502_read   read_func;
   void           *userdata;
} nes6502_memread;

typedef struct nes6502_memwrite_s
{
   uint16_t       min_range, max_range;
   nes6502_write  write_func;
   void           *userdata;
} nes6502_memwrite;


typedef struct nes6502_context_s
{
   /* memory page pointers */
   uint8_t mem[NES6502_MEMSIZE];

   /* user callback, for when cpu hits a JAM instruction */
   void (*jam_callback)(struct nes6502_context_s *context);

   /* memory read/write handlers */
   nes6502_memread* cart_r_handler;
   nes6502_memwrite* cart_w_handler;

   nes6502_memread* io_r_handler;
   int io_r_num;
   nes6502_memwrite* io_w_handler;
   int io_w_num;
  
   /* private data */
   int total_cycles;
   int burn_cycles;
   int remaining_cycles;

   uint16_t pc_reg;
   uint8_t  a_reg, p_reg;
   uint8_t  x_reg, y_reg;
   uint8_t  s_reg;

   bool     jammed;
   bool     int_pending;
} nes6502_context;

/* Functions which govern the 6502's execution */
void nes6502_reset(void);
int nes6502_execute(int total_cycles);
void nes6502_nmi(void);
void nes6502_irq(void);
void nes6502_clearirq(void);
uint8_t nes6502_getbyte(uint16_t address);
uint32_t nes6502_getcycles(bool reset_flag);
void nes6502_burn(int cycles);
void nes6502_release(void);

/* Context get/set */
void nes6502_setcontext(nes6502_context *cpu);
void nes6502_getcontext(nes6502_context *cpu);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif