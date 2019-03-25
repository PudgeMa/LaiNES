#ifndef _CARTRIDGE_NEW_H_
#define _CARTRIDGE_NEW_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define CARTRIDGE_PRG_ROM_SIZE  (32 * 1024)
#define CARTRIDGE_PRG_RAM_SIZE  (8  * 1024)
#define CARTRIDGE_CHR_MEM_SIZE  (8  * 1024)

#define CARTRIDGE_PRG_BANL_SIZE (8 * 1024)
#define CARTRIDGE_CHR_BANK_SIZE (1 * 1024)

#define CARTRIDGE_PRG_BANK_NUM  4
#define CARTRIDGE_CHR_BANK_NUM  8

enum cartridge_prg_page {
    CARTRIDGE_PRG_PAGE_8 = 8,
    CARTRIDGE_PRG_PAGE_16 = 16,
    CARTRIDGE_PRG_PAGE_32 = 32,
};

enum cartridge_chr_page {
    CARTRIDGE_CHR_PAGE_1 = 1,
    CARTRIDGE_CHR_PAGE_2 = 2,
    CARTRIDGE_CHR_PAGE_4 = 4,
    CARTRIDGE_CHR_PAGE_8 = 8,
};

struct cartridge_info
{
    int mapper_num;
    int mirroring;
    int prg_rom_size;
    int prg_rom_pos;
    int chr_mem_size;
    bool chr_mem_ram;
    int chr_rom_pos;
};

struct cartridge_mapper
{
    void (*init)();
    void (*scanline)();
    void (*chr_write)(int addr, uint8_t value);
    void (*prg_write)(int addr, uint8_t value);
};

struct cartridge_mmc
{
    uint8_t* *prg_map;
    uint8_t* *prg_ram;
    uint8_t chr_mem[CARTRIDGE_CHR_MEM_SIZE];
    struct cartridge_mapper mapper;
};

bool cartridge_open(const char *file, struct cartridge_info *info);
bool cartridge_init(struct cartridge_mmc *mmc);
void cartridge_mapPRG(enum cartridge_prg_page size, int targetSlot, int pageNum);
void cartridge_mapCHR(enum cartridge_chr_page size, int targetSlot, int pageNum);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif