#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>

// Глобальные переменные VGA
extern volatile uint16_t* video_memory;
extern int cursor_pos;

extern "C" {
    void update_vga_cursor(int x, int y);
    void print_char(char c); 
    void print(const char* str);
    void println(const char* str); 
    char to_hex(uint8_t val);
    void print_hex_byte(uint8_t byte);
}

#endif // SCREEN_H