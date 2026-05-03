#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

extern bool shift_pressed;
#define CHAR_ARROW_UP    1
#define CHAR_ARROW_DOWN  2
#define CHAR_ARROW_LEFT  3
#define CHAR_ARROW_RIGHT 4
#define CHAR_TAB         9
#define CHAR_SHIFT_TAB   6
#define CHAR_F3          132
#define CHAR_F4          133
#define CHAR_PGUP        5
#define CHAR_PGDN        7
#define CHAR_SHIFT_RIGHT    16
#define CHAR_CTRL_F         134
#define CHAR_DEL            135
#define CHAR_CTRL_Z         136
#define CHAR_CTRL_SHIFT_Z   137
#define CHAR_CTRL_ESC       138
#define CHAR_SHIFT_ESC      139
#define CHAR_CTRL_C         140
#define CHAR_CTRL_V         141
#define CHAR_SHIFT_UP       142
#define CHAR_SHIFT_DOWN     143
#define CHAR_SHIFT_LEFT     144
// CHAR_SHIFT_RIGHT уже = 16, переопределяем нормально:
#undef  CHAR_SHIFT_RIGHT
#define CHAR_SHIFT_RIGHT    145
#define CHAR_CTRL_A         146
#define CHAR_CTRL_E         147
#define CHAR_HOME           148
#define CHAR_END            149

extern "C" uint8_t get_key();

#endif // KEYBOARD_H