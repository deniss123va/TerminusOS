#include "screen.h"
#include "../drivers/disk.h"
#include "../lib/string.h"
#include "Scrollback.h"

// ==========================================
// Глобальные переменные VGA и Темы
// ==========================================
volatile uint16_t* video_memory = (volatile uint16_t*)0xB8000;
int cursor_pos = 0;

// ─── Буфер редиректа (echo text > file) ─────────────────────────────────────
#define REDIRECT_BUF_SIZE 4096
char  redirect_buf[REDIRECT_BUF_SIZE];
int   redirect_len   = 0;
bool  redirect_active = false;

void redirect_start() {
    redirect_len    = 0;
    redirect_buf[0] = 0;
    redirect_active = true;
}
void redirect_stop() {
    redirect_active = false;
    if (redirect_len < REDIRECT_BUF_SIZE)
        redirect_buf[redirect_len] = 0;
}
const char* redirect_get_buf() { return redirect_buf; }
int         redirect_get_len() { return redirect_len; }

// Цветовые константы
#define VGA_COLOR_BLACK 0
#define VGA_COLOR_BLUE 1
#define VGA_COLOR_GREEN 2
#define VGA_COLOR_CYAN 3
#define VGA_COLOR_RED 4
#define VGA_COLOR_MAGENTA 5
#define VGA_COLOR_BROWN 6
#define VGA_COLOR_LIGHT_GREY 7
#define VGA_COLOR_DARK_GREY 8
#define VGA_COLOR_LIGHT_BLUE 9
#define VGA_COLOR_LIGHT_GREEN 10
#define VGA_COLOR_LIGHT_CYAN 11
#define VGA_COLOR_LIGHT_RED 12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_LIGHT_BROWN 14
#define VGA_COLOR_WHITE 15

// Текущая тема (по умолчанию Classic)
uint8_t theme_bg = VGA_COLOR_BLACK;
uint8_t theme_fg = VGA_COLOR_WHITE;
uint8_t theme_bar_bg = VGA_COLOR_LIGHT_GREY;
uint8_t theme_bar_fg = VGA_COLOR_BLACK;
uint8_t theme_cursor = VGA_COLOR_WHITE; // Цвет курсора / символа под ним
uint8_t theme_cursor_bg   = VGA_COLOR_WHITE;  // Фон под курсором
uint8_t theme_cursor_char = VGA_COLOR_BLACK;  // Цвет символа под курсором
uint8_t theme_dir    = VGA_COLOR_LIGHT_CYAN;  // Цвет директорий в ls
uint8_t theme_file   = VGA_COLOR_WHITE;       // Цвет файлов в ls

// ==========================================
// Управление темой
// ==========================================

uint8_t get_theme_color() {
    return (theme_bg << 4) | theme_fg;
}

void set_theme(const char* name) {
    if (strcmp(name, "matrix") == 0) {
        theme_bg          = VGA_COLOR_BLACK;
        theme_fg          = VGA_COLOR_LIGHT_GREEN;
        theme_bar_bg      = VGA_COLOR_GREEN;
        theme_bar_fg      = VGA_COLOR_BLACK;
        theme_cursor      = VGA_COLOR_LIGHT_GREEN;
        theme_cursor_bg   = VGA_COLOR_LIGHT_GREEN;
        theme_cursor_char = VGA_COLOR_BLACK;
        theme_dir         = VGA_COLOR_LIGHT_CYAN;
        theme_file        = VGA_COLOR_LIGHT_GREEN;
    } else if (strcmp(name, "ocean") == 0) {
        theme_bg          = VGA_COLOR_BLUE;
        theme_fg          = VGA_COLOR_WHITE;
        theme_bar_bg      = VGA_COLOR_CYAN;
        theme_bar_fg      = VGA_COLOR_BLUE;
        theme_cursor      = VGA_COLOR_WHITE;
        theme_cursor_bg   = VGA_COLOR_CYAN;
        theme_cursor_char = VGA_COLOR_BLUE;
        theme_dir         = VGA_COLOR_LIGHT_GREEN;
        theme_file        = VGA_COLOR_WHITE;
    } else if (strcmp(name, "amber") == 0) {
        theme_bg          = VGA_COLOR_BLACK;
        theme_fg          = VGA_COLOR_LIGHT_BROWN;
        theme_bar_bg      = VGA_COLOR_BROWN;
        theme_bar_fg      = VGA_COLOR_BLACK;
        theme_cursor      = VGA_COLOR_LIGHT_BROWN;
        theme_cursor_bg   = VGA_COLOR_LIGHT_BROWN;
        theme_cursor_char = VGA_COLOR_BLACK;
        theme_dir         = VGA_COLOR_LIGHT_GREEN;
        theme_file        = VGA_COLOR_LIGHT_BROWN;
    } else if (strcmp(name, "bsod") == 0) {
        theme_bg          = VGA_COLOR_BLUE;
        theme_fg          = VGA_COLOR_WHITE;
        theme_bar_bg      = VGA_COLOR_WHITE;
        theme_bar_fg      = VGA_COLOR_BLUE;
        theme_cursor      = VGA_COLOR_WHITE;
        theme_cursor_bg   = VGA_COLOR_WHITE;
        theme_cursor_char = VGA_COLOR_BLUE;
        theme_dir         = VGA_COLOR_LIGHT_CYAN;
        theme_file        = VGA_COLOR_WHITE;
    } else if (strcmp(name, "red") == 0) {
        theme_bg          = VGA_COLOR_BLACK;
        theme_fg          = VGA_COLOR_RED;
        theme_bar_bg      = VGA_COLOR_RED;
        theme_bar_fg      = VGA_COLOR_WHITE;
        theme_cursor      = VGA_COLOR_LIGHT_RED;
        theme_cursor_bg   = VGA_COLOR_LIGHT_RED;
        theme_cursor_char = VGA_COLOR_BLACK;
        theme_dir         = VGA_COLOR_LIGHT_MAGENTA;
        theme_file        = VGA_COLOR_RED;
    } else { // default / classic
        theme_bg          = VGA_COLOR_BLACK;
        theme_fg          = VGA_COLOR_WHITE;
        theme_bar_bg      = VGA_COLOR_LIGHT_GREY;
        theme_bar_fg      = VGA_COLOR_BLACK;
        theme_cursor      = VGA_COLOR_WHITE;
        theme_cursor_bg   = VGA_COLOR_WHITE;
        theme_cursor_char = VGA_COLOR_BLACK;
        theme_dir         = VGA_COLOR_LIGHT_CYAN;
        theme_file        = VGA_COLOR_WHITE;
    }
}

// ==========================================
// VGA функции
// ==========================================

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

    // Фон = theme_cursor_bg, символ = theme_cursor_char
    // Если под курсором пробел/пусто — рисуем '_'
    uint8_t cursor_attr = (theme_cursor_bg << 4) | (theme_cursor_char & 0x0F);

    if (ch == ' ' || ch == 0) {
        ch = '_';
    }

    video_memory[pos] = ((uint16_t)cursor_attr << 8) | ch;
}

void clear_block_cursor(int x, int y) {
    int pos = y * 80 + x;
    uint16_t current = video_memory[pos];
    uint8_t ch = current & 0xFF;

    // Восстанавливаем обычный цвет
    uint8_t normal_color = get_theme_color();

    if (ch == '_') {
        ch = ' ';
    }

    video_memory[pos] = (normal_color << 8) | ch;
}

void print_char(char c) {
    // Если активен редирект — пишем в буфер, не на экран
    if (redirect_active) {
        if (c != '\b' && redirect_len < REDIRECT_BUF_SIZE - 1)
            redirect_buf[redirect_len++] = c;
        return;
    }

    uint8_t attr = get_theme_color();
    uint16_t blank = (attr << 8) | ' ';

    if(c == '\n'){
        int row = cursor_pos / 80;
        cursor_pos = (row + 1) * 80;
    } else if (c == '\r') {
        // ignore
    } else {
        video_memory[cursor_pos++] = (attr << 8) | c;
    }
    
    // ✅ ПРАВИЛЬНАЯ ПРОКРУТКА с учетом цвета
    while (cursor_pos >= 80 * 25) { 
        // Сохраняем статус-бар (строка 0)
        uint16_t statusbar_backup[80];
        for (int i = 0; i < 80; i++) {
            statusbar_backup[i] = video_memory[i];
        }

        // Сохраняем строку 1 в scrollback перед тем как она уедет
        scrollback_push((const uint16_t*)(video_memory + 80));

        // Прокручиваем ВСЕ строки 1-24 вверх
        for (int row = 1; row < 24; row++) {
            for (int col = 0; col < 80; col++) {
                video_memory[row * 80 + col] = video_memory[(row + 1) * 80 + col];
            }
        }
        
        // Очищаем последнюю строку (строка 24) цветом темы
        for (int i = 80 * 24; i < 80 * 25; i++) {
            video_memory[i] = blank;
        }

        // Восстанавливаем статус-бар
        for (int i = 0; i < 80; i++) {
            video_memory[i] = statusbar_backup[i];
        }
        
        cursor_pos -= 80; 
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

void set_custom_theme(uint8_t bg, uint8_t fg, uint8_t bar_bg, uint8_t bar_fg,
                      uint8_t cursor, uint8_t cursor_bg, uint8_t cursor_char,
                      uint8_t dir, uint8_t file) {
    theme_bg          = bg          & 0x0F;
    theme_fg          = fg          & 0x0F;
    theme_bar_bg      = bar_bg      & 0x0F;
    theme_bar_fg      = bar_fg      & 0x0F;
    theme_cursor      = cursor      & 0x0F;
    theme_cursor_bg   = cursor_bg   & 0x0F;
    theme_cursor_char = cursor_char & 0x0F;
    theme_dir         = dir         & 0x0F;
    theme_file        = file        & 0x0F;
}

void print_char_colored(char c, uint8_t color) {
    // Печатает символ с произвольным атрибутом (не меняет cursor_pos логику)
    if (redirect_active) { print_char(c); return; }
    if (c == '\n') { print_char(c); return; }
    uint8_t saved_fg = theme_fg;
    // Временно меняем fg чтобы переиспользовать прокрутку из print_char
    // Проще: напрямую пишем в видеопамять с нужным цветом
    if (cursor_pos < 80 * 25) {
        video_memory[cursor_pos++] = ((uint16_t)color << 8) | (uint8_t)c;
        update_vga_cursor(cursor_pos % 80, cursor_pos / 80);
    }
}