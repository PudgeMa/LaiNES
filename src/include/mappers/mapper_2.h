#ifndef _MAPPER_2_H_
#define _MAPPER_2_H_

#include "cartridge_new.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void mapper_2_init(struct cartridge_info *info);
void mapper_2_prg_write(int addr, uint8_t value);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MAPPER_2_H_ */