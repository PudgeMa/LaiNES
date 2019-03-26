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

#define CARTRIDGE_PRG_BANK_SIZE (8 * 1024)
#define CARTRIDGE_PRG_BANK_NUM  4

enum cartridge_window {
    CARTRIDGE_WINDOW_CHR_1 = 1,
    CARTRIDGE_WINDOW_CHR_2 = 2,
    CARTRIDGE_WINDOW_CHR_4 = 4,
    CARTRIDGE_WINDOW_CHR_8 = 8,

    CARTRIDGE_WINDOW_PRG_8  = 8,
    CARTRIDGE_WINDOW_PRG_16 = 16,
    CARTRIDGE_WINDOW_PRG_32 = 32,
};

enum cartridge_prg_mirror {
    CARTRIDGE_MIRROR_8 = 8,
    CARTRIDGE_MIRROR_16 = 16,
    CARTRIDGE_MIRROR_32 = 32
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
    void (*init)(struct cartridge_info *info);
    void (*scanline)();
    void (*chr_write)(int addr, uint8_t value);
    void (*prg_write)(int addr, uint8_t value);
};

struct cartridge_mmc
{
    uint8_t* *prg_map;
    uint8_t* *prg_ram;
    uint8_t*  chr_mem;
    struct cartridge_mapper mapper;
};

bool cartridge_open(const char *file, struct cartridge_info *info);
bool cartridge_init(struct cartridge_mmc *mmc);
void cartridge_mirrorPRG(enum cartridge_prg_mirror mode);
void cartridge_bankPRG(enum cartridge_window size, int target, int window);
void cartridge_bankCHR(enum cartridge_window size, int target, int window);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif