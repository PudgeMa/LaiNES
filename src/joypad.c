#include "joypad.h"
#include "nes.hpp"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define JOYPAD_PORT_NUM 2

static int ports[JOYPAD_PORT_NUM];
static bool strobe;

uint8_t joypad_handler_read(void *userdata, uint16_t address)
{
    int p = address & 1;
    // When strobe is high, it keeps reading A
    if (strobe) {
        nes_updateJoypad(p, &ports[p]);
        return 0x40 | ( ports[p] & 1);
    }
    // Get the status of a button and shift the register
    uint8_t j = 0x40 | (ports[p] & 1);
    ports[p] = 0x80 | (ports[p] >> 1);
    return j;
}

void joypad_handler_write(void *userdata, uint16_t address, uint8_t value)
{
    bool n = (value != 0);
    if (!n && strobe) {
        nes_updateJoypad(NES_JOYPAD_0, &ports[0]);
        nes_updateJoypad(NES_JOUPAD_1, &ports[1]);
    }
    strobe = n; 
}