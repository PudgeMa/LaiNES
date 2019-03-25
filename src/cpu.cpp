#include <cstdlib>
#include <cstring>
#include "cartridge_new.h"
#include "ppu.hpp"
#include "cpu.hpp"
#include "nes6502.h"
#include "dis6502.h"
#include "joypad.h"
#include "gui.hpp"

namespace CPU {

const int writehandlers_num = 4;

#define NES_SCANLINE_CYCLES 113

u8 mem[0x800];
long current_cycles = 0;

nes6502_context context;
struct cartridge_mmc mmc;

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

void mapper_prg_write(void *userdata, uint16_t offset, uint8_t value) {
    mmc.mapper.prg_write(offset, value);
}

nes6502_memread readhandlers[] = {
    {
        .min_range = 0x2000,
        .max_range = 0x3FFF,
        .read_func = ppu_read,
        .userdata = nullptr
    }, {
        .min_range = 0x4016,
        .max_range = 0x4017,
        .read_func = joypad_handler_read,
        .userdata = nullptr
    }
};

const int readhandlers_num = 2;

nes6502_memwrite writehandlers[] = {
    {
        .min_range = 0x2000, 
        .max_range = 0x3FFF,
        .write_func = ppu_write,
        .userdata = nullptr
    }, {
        .min_range = 0x4014, 
        .max_range = 0x4014,
        .write_func = dma_write,
        .userdata = nullptr
    }, {
        .min_range = 0x4016,
        .max_range = 0x4016,
        .write_func = joypad_handler_write,
        .userdata = nullptr
    }, {
        .min_range = 0x8000,
        .max_range = 0xFFFF,
        .write_func = mapper_prg_write,
        .userdata = nullptr
    }
};

void power(struct cartridge_info *info)
{

    memset(mem, 0, sizeof(mem));

    context.mem_page[0] = mem;


    context.jam_callback = cpu_jam;
    context.read_handler= readhandlers;
    context.read_num = readhandlers_num;
    context.write_handler = writehandlers;
    context.write_num = writehandlers_num;

    mmc.prg_ram = &context.mem_page[3];
    mmc.prg_map = &context.mem_page[4];

    cartridge_init(&mmc);

    PPU::init(&mmc);
    PPU::set_mirroring(info->mirroring ? PPU::VERTICAL : PPU::HORIZONTAL);

    nes6502_setcontext(&context);
    nes6502_reset();
    current_cycles = NES_SCANLINE_CYCLES;
}

u32 pixels[256 * 240];     // Video buffer.

static inline void execute()
{
	if (current_cycles > 0)
			current_cycles -= nes6502_execute(current_cycles);
	current_cycles += NES_SCANLINE_CYCLES;
}

void run_frame()
{
    for (int i = 0; i < 240; ++i)
    {
        execute();
		PPU::scanline_visible(i, pixels + (i * 256));
    }
    GUI::new_frame(pixels);
    for(int i = 240; i <= 261; ++i)
    {
        execute();
		PPU::scanline_other(i);
    }
}

}
