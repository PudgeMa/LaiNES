#include <cstdlib>
#include <cstring>
#include <iostream>
#include "apu.hpp"
#include "cartridge.hpp"
#include "joypad.hpp"
#include "ppu.hpp"
#include "cpu.hpp"
#include "nes6502.h"
#include "dis6502.h"

namespace CPU {

int dmc_read(void*, cpu_addr_t addr) 
{
    return 0;
}
 

void set_nmi(bool v) 
{ 
    nes6502_nmi();
}

void set_irq(bool v) 
{
    if (v) {
        nes6502_irq();
    } else {
        nes6502_clearirq();
    }
}

static void cpu_jam(nes6502_context *pContext)
{
   fprintf(stderr, "execution halted due to cpu jam\n");
   fprintf(stderr, "%s", nes6502_disasm(pContext->pc_reg - 1, pContext->p_reg,
                                  pContext->a_reg, pContext->x_reg,
                                  pContext->y_reg, pContext->s_reg));
   exit(-1);
}

uint8_t ppu_read(void *userdata, uint16_t offset) {
    return PPU::access<0>(offset % 8, 0);
}

void ppu_write(void *userdata, uint16_t offset, uint8_t value) {
    PPU::access<1>(offset % 8, value);
}

void dma_write(void *userdata, uint16_t offset, uint8_t value) {
    nes6502_release();
    nes6502_burn(512);
    for (int i = 0; i < 256; ++i) {
        ppu_write(nullptr, 0x2014, nes6502_getbyte(value * 256 + i));
    }
}

uint8_t cart_read(void *userdata, uint16_t offset) {
    return Cartridge::access<0>(offset, 0);
}

void cart_write(void *userdata, uint16_t offset, uint8_t value) {
    Cartridge::access<1>(offset, value);
}

nes6502_memread readhandlers[] = {
    {
        .min_range = 0x2000,
        .max_range = 0x3FFF,
        .read_func = ppu_read,
        .userdata = nullptr
    }
};

const int readhandlers_num = 1;

nes6502_memwrite writehandlers[] = {
    {
        .min_range = 0x2000, 
        .max_range = 0x3FFF,
        .write_func = ppu_write,
        .userdata = nullptr
    },
    {
        .min_range = 0x4014, 
        .max_range = 0x4014,
        .write_func = dma_write,
        .userdata = nullptr
    }
};

const int writehandlers_num = 2;

nes6502_memread cart_r = {
    .min_range = 0x6000,
    .max_range = 0xFFFF,
    .read_func = cart_read,
    .userdata = nullptr
};

nes6502_memwrite cart_w = {
    .min_range = 0x6000,
    .max_range = 0xFFFF,
    .write_func = cart_write,
    .userdata = nullptr
};

void power()
{
    nes6502_context context;
    memset(&context, 0, sizeof(nes6502_context));
    context.jam_callback = cpu_jam;
    context.io_r_handler = readhandlers;
    context.io_r_num = readhandlers_num;
    context.io_w_handler = writehandlers;
    context.io_w_num = writehandlers_num;
    context.cart_r_handler = &cart_r;
    context.cart_w_handler = &cart_w;
    nes6502_setcontext(&context);
    nes6502_reset();
}

const int TOTAL_CYCLES = 29781;
int remainingCycles;

void run_frame()
{
    remainingCycles += TOTAL_CYCLES;

    while (remainingCycles > 0) {
        int c = nes6502_execute(1);
        for(int i = 0; i < 3 * c; ++i) {
            PPU::step();
        }
        remainingCycles -= c;
    }
}

}
