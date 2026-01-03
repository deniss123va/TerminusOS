#ifndef SHELL_H
#define SHELL_H

#include "../drivers/commands/cmd_nano.h"
#include <stdint.h>

#define BUF_SIZE 256
#define HISTORY_SIZE 10
#define STATUS_BAR_ROW 0  // Верхняя строка для GUI

extern char buffer[BUF_SIZE];
extern int buf_len;
extern int cursor_offset;
extern char current_path[128];
extern char history[HISTORY_SIZE][BUF_SIZE];
extern int history_count;
extern int history_index;
extern int last_cursor_x;
extern int last_cursor_y;

// GUI Functions
void shell_draw_status_bar();  // Рисует верхнюю панель
void shell_update_time();       // Обновляет время справа
void shell_update_command();    // Обновляет команду слева

// Shell Functions
void shell_main();
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
