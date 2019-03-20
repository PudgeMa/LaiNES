#ifndef _JOYPAD_H_
#define _JOYPAD_H_
#include "nes6502.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

uint8_t joypad_handler_read(void *userdata, uint16_t address);
void joypad_handler_write(void *userdata, uint16_t address, uint8_t value);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif