#ifndef SHELL_H
#define SHELL_H

#include "../drivers/commands/cmd_nano.h"
#include <stdint.h>

#define BUF_SIZE 256
#define HISTORY_SIZE 10

extern char buffer[BUF_SIZE];
extern int buf_len;
extern int cursor_offset;
extern char current_path[128];
extern char history[HISTORY_SIZE][BUF_SIZE];
extern int history_count;
extern int history_index;

extern int last_cursor_x;
extern int last_cursor_y;

// Функции
void shell_main();   // ✅ Добавлено объявление
void shell_print_prompt();
void shell_clear_line();
void shell_redraw();
void shell_insert_char(char c);
void shell_delete_char();
void shell_add_to_history(const char* cmd);
void shell_load_history(int index);
void process_command();
void shell_init_cursor();

#endif
