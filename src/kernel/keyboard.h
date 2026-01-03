#ifndef KEYBOARD_H
#define KEYBOARD_H

extern bool shift_pressed;
#define CHAR_ARROW_UP    1
#define CHAR_ARROW_DOWN  2
#define CHAR_ARROW_LEFT  3
#define CHAR_ARROW_RIGHT 4
#define CHAR_F3          132
#define CHAR_F4          133

extern "C" char get_key();

#endif // KEYBOARD_H