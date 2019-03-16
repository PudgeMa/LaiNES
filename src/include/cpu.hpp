#pragma once
#include "common.hpp"
#include <Nes_Apu.h>

namespace CPU {


enum IntType { NMI, RESET, IRQ, BRK };  // Interrupt type.

void set_nmi(bool v = true);
void set_irq(bool v = true);
int dmc_read(void*, cpu_addr_t addr);
void power();
void run_frame();

}
