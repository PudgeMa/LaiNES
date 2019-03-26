#include "mappers/mapper_0.h"

void mapper_0_init(struct cartridge_info *info)
{
    if (info->prg_rom_size == (32 * 1024)) {
        cartridge_mirrorPRG(CARTRIDGE_MIRROR_32);
        cartridge_bankPRG(CARTRIDGE_WINDOW_PRG_32, 0, 0);
    } else {
        cartridge_mirrorPRG(CARTRIDGE_MIRROR_16);
        cartridge_bankPRG(CARTRIDGE_WINDOW_PRG_16, 0, 0);   
    }
    cartridge_bankCHR(CARTRIDGE_WINDOW_CHR_8, 0, 0);
}

