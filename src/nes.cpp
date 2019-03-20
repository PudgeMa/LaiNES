#include "nes.hpp"
#include "gui.hpp"

void nes_updateJoypad(nes_joypad_type type, int* value) 
{
    uint8_t v = GUI::get_joypad_state(type);
    *value = v;
}