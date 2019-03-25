#include "mappers/mapper_3.h"

uint8_t reg;

void mapper_3_init()
{
    cartridge_mapPRG(CARTRIDGE_PRG_PAGE_32, 0, 0);
    cartridge_mapCHR(CARTRIDGE_CHR_PAGE_8, 0, 0);
}

void mapper_3_prg_write(int addr, uint8_t value)
{
    cartridge_mapCHR(CARTRIDGE_CHR_PAGE_8, 0, value & 0b11);
}