#ifndef KEYBOARD_H
#define KEYBOARD_H

extern bool shift_pressed; // Глобальное состояние Shell
#define CHAR_ARROW_UP 1
#define CHAR_ARROW_DOWN 2
#define CHAR_ARROW_LEFT 3
#define CHAR_ARROW_RIGHT 4
#define CHAR_ESC 27

extern "C" char get_key();

#endif // KEYBOARD_H