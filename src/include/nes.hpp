#ifndef _NES_H_
#define _NES_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
    NES_JOYPAD_0,
    NES_JOUPAD_1
} nes_joypad_type;

typedef enum {
    NES_JOYPAD_KEY_A = 0,
    NES_JOYPAD_KEY_B,
    NES_JOYPAD_KEY_SELECT,
    NES_JOYPAD_KEY_START,
    NES_JOYPAD_KEY_UP,
    NES_JOYPAD_KEY_DOWN,
    NES_JOYPAD_KEY_LEFT,
    NES_JOYPAD_KEY_RIGHT,
} nes_joypad_key;

typedef enum {
    NES_JOYPAD_RELEASE = 0,
    NES_JOYPAD_PRESS,
} nes_joypad_state;

void nes_updateJoypad(nes_joypad_type type, int* value);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif