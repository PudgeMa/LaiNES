/*
// shatbox (C) 2002 Matthew Conte (matt@conte.com)
**
**
** dis6502.h
**
** 6502 disassembler
**
** $Id: $
*/

#ifndef _DIS6502_H_
#define _DIS6502_H_

#include "nes6502.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern char *nes6502_disasm(uint32_t PC, uint8_t P, uint8_t A, uint8_t X, uint8_t Y, uint8_t S);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_DIS6502_H_ */
