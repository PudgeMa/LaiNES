#include "cartridge_new.h"
#include "iNES.h"
#include "mappers/mapper_0.h"
#include "mappers/mapper_3.h"
#include "mappers/mapper_default.h"
#include <stdio.h>

uint8_t prg_rom[CARTRIDGE_PRG_ROM_SIZE];
uint8_t prg_ram[CARTRIDGE_PRG_RAM_SIZE];

FILE* game_file;
struct cartridge_info *game_info;
struct cartridge_mmc *game_mmc;

bool cartridge_open(const char *path, struct cartridge_info *info)
{
    uint8_t header_buffer[16];
    game_file = fopen(path, "rb");
    if (game_file == NULL) {
        printf("cartridge, open fail\n");
        return false;
    }
    size_t s = fread(header_buffer, 16, 1, game_file);
    if (s != 1) {
        printf("cartridge, read file fail(%ld)\n", s);
        return false;
    }
    if (!iNES_parseHeader(info, header_buffer)) {
        return false;
    }
    printf("cartridge, mapper(%d), mirroring(%d), prg_size(%d), prg_pos(%d) ",
            info->mapper_num, info->mirroring, info->prg_rom_size, info->prg_rom_pos);
    printf("chr_ram(%d), chr_size(%d), chr_pos(%d)\n",
            info->chr_mem_ram, info->chr_mem_size, info->chr_rom_pos);
    game_info = info;
    return true;
}

bool cartridge_init(struct cartridge_mmc *mmc)
{
    struct cartridge_mapper *mapper = &mmc->mapper;
    mapper->scanline = mapper_d_scanline;
    mapper->chr_write = mapper_d_chr_write;
    mapper->prg_write = mapper_d_prg_write;

    switch (game_info->mapper_num) {
    case 0:
        mapper->init = mapper_0_init;
        break;
    case 3:
        mapper->init = mapper_3_init;
        mapper->prg_write = mapper_3_prg_write;
        break;
    default:
        printf("not support mapper %d\n", game_info->mapper_num);
        return false;
    }
    mmc->prg_ram[0] = prg_ram;

    // fseek(game_file, game_info->prg_rom_pos, SEEK_SET);
    // int size = fread(prg_rom, game_info->prg_rom_size, 1, game_file);
    // fseek(game_file, game_info->chr_rom_pos, SEEK_SET);
    // int size = fread(chr_mem, game_info->chr_mem_size, 1, game_file);

    game_mmc = mmc;
    mapper->init();
    return true;
}

void cartridge_mapPRG(enum cartridge_prg_page size, int targetSlot, int pageNum)
{
    printf("cartridge_mapPRG(%d, %d, %d)\n", size, targetSlot, pageNum);
    int bankOfSlot = size / 8;
    int pageSize = size * 1024;
    /* read content from file */
    int memPos = pageSize * targetSlot;
    fseek(game_file, game_info->prg_rom_pos + pageNum * pageSize, SEEK_SET);
    int readSize = game_info->prg_rom_size > pageSize ? pageSize : game_info->prg_rom_size;
    fread(prg_rom + memPos, readSize, 1, game_file);

    for (int i = 0; i < bankOfSlot; ++i) {
        game_mmc->prg_map[bankOfSlot * targetSlot + i]
            = prg_rom + 
            ((pageSize * pageNum + CARTRIDGE_PRG_BANL_SIZE * i) % game_info->prg_rom_size);
    }
}

void cartridge_mapCHR(enum cartridge_chr_page size, int targetSlot, int pageNum)
{
    printf("cartridge_mapCHR(%d, %d, %d)\n", size, targetSlot, pageNum);
    int pageSize = size * 1024;    
    int memPos = pageSize * targetSlot;
    int filePos = pageSize * pageNum;

    if (game_info->chr_mem_size < filePos + pageSize) {
        printf("error!, can't read %d bytes at %d\n", filePos, pageSize);
    }

    fseek(game_file, game_info->chr_rom_pos + filePos, SEEK_SET);
    fread(game_mmc->chr_mem + memPos, pageSize, 1, game_file);
}
