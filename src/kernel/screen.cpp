#include "screen.h"
#include "../drivers/disk.h" // Для inb/outb

volatile uint16_t* video_memory = (volatile uint16_t*)0xB8000;
int cursor_pos = 0;

void update_vga_cursor(int x, int y) {
    int pos = y * 80 + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void print_char(char c) {
    if(c == '\n'){
        int row = cursor_pos / 80;
        cursor_pos = (row + 1) * 80;
    } else if (c == '\r') {
        // Игнорируем CR
    } else {
        video_memory[cursor_pos++] = c | 0x0F00;
    }
    
    // ИСПРАВЛЕНО: Прокрутка 
    if (cursor_pos >= 80 * 25) { 
        for(int i = 0; i < 80 * 24; i++) {
            video_memory[i] = video_memory[i + 80];
        }
        for(int i = 80 * 24; i < 80 * 25; i++) {
            video_memory[i] = 0x0F00;
        }
        cursor_pos = 80 * 24;
    }
    update_vga_cursor(cursor_pos % 80, cursor_pos / 80);
}

void print(const char* str) {
    int i=0;
    while(str[i]) print_char(str[i++]);
}

void println(const char* str) {
    print(str);
    print_char('\n');
}

char to_hex(uint8_t val) {
    if (val < 10) return val + '0';
    return val - 10 + 'A';
}

void print_hex_byte(uint8_t byte) {
    print_char(to_hex((byte >> 4) & 0x0F));
    print_char(to_hex(byte & 0x0F));
}