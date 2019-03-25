#ifndef _MAPPER_DEFAULT_H_
#define _MAPPER_DEFAULT_H_

#include "cartridge_new.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void mapper_d_scanline();
void mapper_d_chr_write(int addr, uint8_t value);
void mapper_d_prg_write(int addr, uint8_t value);

#ifdef __cplusplus
}
#endif /* __cplusplus */



#endif /* MAPPER_DEFAULT_H_ */
