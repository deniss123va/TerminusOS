#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

struct MouseState {
    int x, y;
    uint8_t buttons;
    // Сырая дельта ИЗ ПОСЛЕДНЕГО ПАКЕТА, до клампа x/y по границам экрана.
    // Использовать для скролл-жестов (MMB) вместо diff(x_prev, x_now) —
    // тот diff даёт 0 на краю экрана, даже если мышь физически продолжает
    // двигаться, и жест "залипает".
    int raw_dx, raw_dy;
};

extern MouseState mouse_state;

void mouse_init();
void mouse_handle_packet();

#endif // MOUSE_H
