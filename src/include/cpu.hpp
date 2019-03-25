#pragma once
#include "common.hpp"
#include "cartridge_new.h"

namespace CPU {

enum IntType { NMI, RESET, IRQ, BRK };  // Interrupt type.

void set_nmi(bool v = true);
void set_irq(bool v = true);
void power(struct cartridge_info *info);
void run_frame();
}
