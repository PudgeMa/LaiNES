#ifndef _MAPPER_3_H_
#define _MAPPER_3_H_

#include "cartridge_new.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void mapper_3_init();
void mapper_3_prg_write(int addr, uint8_t value);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MAPPER_3_H_ */
