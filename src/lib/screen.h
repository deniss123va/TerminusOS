#ifndef SCREEN_H
#define SCREEN_H
#define STATUS_BAR_ROW 0
#include <stdint.h>

// Глобальные переменные VGA
extern volatile uint16_t* video_memory;
extern int cursor_pos;

// ─── Редирект вывода (echo > file) ───────────────────────────────────────────
#define REDIRECT_BUF_SIZE 4096
extern char  redirect_buf[REDIRECT_BUF_SIZE];
extern int   redirect_len;
extern bool  redirect_active;
void redirect_start();
void redirect_stop();
const char* redirect_get_buf();
int         redirect_get_len();

extern uint8_t theme_bg;
extern uint8_t theme_fg;
extern uint8_t theme_bar_bg;
extern uint8_t theme_bar_fg;
extern uint8_t theme_cursor;
extern uint8_t theme_cursor_bg;
extern uint8_t theme_cursor_char;
extern uint8_t theme_dir;
extern uint8_t theme_file;

void set_theme(const char* name);
uint8_t get_theme_color();

extern "C" {
    void disable_vga_cursor();
    void draw_block_cursor(int x, int y);
    void clear_block_cursor(int x, int y);
    void update_vga_cursor(int x, int y);
    void print_char(char c); 
    void print_char_colored(char c, uint8_t color);
    void print(const char* str);
    void println(const char* str); 
    char to_hex(uint8_t val);
    void print_hex_byte(uint8_t byte);
    void shell_init_status_bar();
    void set_custom_theme(uint8_t bg, uint8_t fg, uint8_t bar_bg, uint8_t bar_fg,
                          uint8_t cursor, uint8_t cursor_bg, uint8_t cursor_char,
                          uint8_t dir, uint8_t file);

}

#endif // SCREEN_H