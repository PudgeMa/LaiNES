#include "cartridge_new.h"
#include "iNES.h"
#include "mappers/mapper_0.h"
#include "mappers/mapper_2.h"
#include "mappers/mapper_3.h"
#include "mappers/mapper_default.h"
#include <stdio.h>

uint8_t prg_rom[CARTRIDGE_PRG_ROM_SIZE];
uint8_t prg_ram[CARTRIDGE_PRG_RAM_SIZE];
uint8_t chr_mem[CARTRIDGE_CHR_MEM_SIZE];

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
    case 2:
        mapper->init = mapper_2_init;
        mapper->prg_write = mapper_2_prg_write;
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
    mmc->chr_mem = chr_mem;
    game_mmc = mmc;
    mapper->init(game_info);
    return true;
}

void cartridge_bankPRG(enum cartridge_window size, int target, int window)
{
    printf("cartridge_bankPRG(%d, %d, %d)\n", size, target, window);
    int windowSize = size * 1024;
    int memPos = windowSize * target;
    int filePos = windowSize * window;
    
    if (game_info->prg_rom_size < filePos + windowSize) {
        printf("error!, can't read %d bytes at %d\n", filePos, windowSize);
    }

    fseek(game_file, game_info->prg_rom_pos + filePos, SEEK_SET);
    if (1 != fread(prg_rom + memPos, windowSize, 1, game_file)) {
        printf("error!, read fail\n");
    }
}

void cartridge_mirrorPRG(enum cartridge_prg_mirror mode)
{
    int diffBank = mode / 8;
    int slotNumber = 32 / mode;
    for(int i = 0; i < slotNumber; ++i) {
        for (int j = 0; j < diffBank; ++j) {
            game_mmc->prg_map[i * slotNumber + j] = prg_rom + j * CARTRIDGE_PRG_BANK_SIZE;
        }
    }
}

void cartridge_bankCHR(enum cartridge_window size, int target, int window)
{
    printf("cartridge_bankCHR(%d, %d, %d)\n", size, target, window);
    int windowSize = size * 1024;    
    int memPos = windowSize * target;
    int filePos = windowSize * window;

    if (game_info->chr_mem_size < filePos + windowSize) {
        printf("error!, can't read %d bytes at %d\n", filePos, windowSize);
    }

    fseek(game_file, game_info->chr_rom_pos + filePos, SEEK_SET);
    fread(game_mmc->chr_mem + memPos, windowSize, 1, game_file);
}
