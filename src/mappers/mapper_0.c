#include "mappers/mapper_0.h"

void mapper_0_init()
{
    cartridge_mapPRG(CARTRIDGE_PRG_PAGE_32, 0, 0);
    cartridge_mapCHR(CARTRIDGE_CHR_PAGE_8, 0, 0);
}

