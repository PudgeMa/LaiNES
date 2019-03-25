/*
 * iNES.h
 * iNES file format
 *  Created on: 2019年3月24日
 *      Author: maruipu
 */

#ifndef _INES_H_
#define _INES_H_

#include <stdint.h>
#include "cartridge_new.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define INES_PRG_ROM_UNIT (16 * 1024)
#define INES_CHR_ROM_UNIT (8  * 1024)

bool iNES_parseHeader(struct cartridge_info* info, uint8_t data[16]);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _INES_H_ */
