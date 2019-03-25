#include "iNES.h"
#include <string.h>
#include <stdio.h>

bool iNES_parseHeader(struct cartridge_info* info, uint8_t data[16])
{
    if(memcmp(data, "NES\x1a", 4)) {
        puts("iNES, wrong magic chunk");
        return false;
    }
    info->mapper_num = (data[7] & 0xF0) | (data[6] >> 4);
    info->mirroring = data[6] & 1; /* TODO: bit 3 indicates four-screen mirroring */
    info->prg_rom_size = data[4] * INES_PRG_ROM_UNIT;
    int chr_size = data[5] * INES_CHR_ROM_UNIT;
    info->chr_mem_ram = (chr_size == 0);
    info->chr_mem_size = (chr_size == 0 ? INES_CHR_ROM_UNIT : chr_size);
    bool trainer = data[6] & 0b100;
    info->prg_rom_pos = trainer ? (16 + 512) : 16;
    info->chr_rom_pos = info->prg_rom_pos + info->prg_rom_size;
    return true;
}
