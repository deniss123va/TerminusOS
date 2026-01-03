#include "screen.h"
#include "../drivers/disk.h"

volatile uint16_t* video_memory = (volatile uint16_t*)0xB8000;
int cursor_pos = 0;

void disable_vga_cursor() {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void update_vga_cursor(int x, int y) {
    int pos = y * 80 + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void draw_block_cursor(int x, int y) {
    int pos = y * 80 + x;
    uint16_t current = video_memory[pos];
    uint8_t ch = current & 0xFF;
    uint8_t color = (current >> 8) & 0xFF;
    uint8_t fg = color & 0x0F;
    uint8_t bg = (color >> 4) & 0x0F;
    uint8_t inverted_color = (fg << 4) | bg;
    
    if (ch == ' ' || ch == 0) {
        ch = 219;
    }
    video_memory[pos] = (inverted_color << 8) | ch;
}

void clear_block_cursor(int x, int y) {
    int pos = y * 80 + x;
    uint16_t current = video_memory[pos];
    uint8_t ch = current & 0xFF;
    if (ch == 219) {
        ch = ' ';
    }
    video_memory[pos] = 0x0F00 | ch;
}

void print_char(char c) {
    if(c == '\n'){
        int row = cursor_pos / 80;
        cursor_pos = (row + 1) * 80;
    } else if (c == '\r') {
        // пусто
    } else {
        video_memory[cursor_pos++] = c | 0x0F00;
    }

    // ✅ ПРАВИЛЬНАЯ ПРОКРУТКА с while
    while (cursor_pos >= 80 * 25) {  // Когда дошли до строки 25
        // Сохраняем статус-бар (строка 0)
        uint16_t statusbar_backup[80];
        for (int i = 0; i < 80; i++) {
            statusbar_backup[i] = video_memory[i];
        }

        // Прокручиваем ВСЕ строки 1-24 вверх
        for (int row = 1; row < 24; row++) {
            for (int col = 0; col < 80; col++) {
                video_memory[row * 80 + col] = video_memory[(row + 1) * 80 + col];
            }
        }

        // Очищаем последнюю строку (строка 24)
        for (int i = 80 * 24; i < 80 * 25; i++) {
            video_memory[i] = 0x0F00;
        }

        // Восстанавливаем статус-бар
        for (int i = 0; i < 80; i++) {
            video_memory[i] = statusbar_backup[i];
        }

        cursor_pos -= 80;  // ✅ Отнимаем одну строку
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
