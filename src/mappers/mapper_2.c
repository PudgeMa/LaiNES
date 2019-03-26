#include "mappers/mapper_2.h"

int reg;

void mapper_2_init(struct cartridge_info *info)
{
    cartridge_mirrorPRG(CARTRIDGE_MIRROR_1);
    int bankNumber = info->prg_rom_size / (CARTRIDGE_WINDOW_PRG_16 * 1024);
    cartridge_bankPRG(CARTRIDGE_WINDOW_PRG_16, 0, 0);
    cartridge_bankPRG(CARTRIDGE_WINDOW_PRG_16, 1, bankNumber - 1);
    
    cartridge_bankCHR(CARTRIDGE_WINDOW_CHR_8, 0, 0);
    reg = 0;
}

void mapper_2_prg_write(int addr, uint8_t value)
{
    int target = value & 0x0F;
    if (target != reg) {
        cartridge_bankPRG(CARTRIDGE_WINDOW_PRG_16, 0, target);
        reg = target;
    }
}

