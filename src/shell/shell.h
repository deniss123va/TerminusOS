#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>

// Глобальное состояние Shell и Курсор ввода
#define BUF_SIZE 128
#define HISTORY_SIZE 10

extern char buffer[BUF_SIZE];
extern int buf_len; // Длина введенной строки
extern int cursor_offset; // Смещение курсора внутри buffer (для редактирования)

extern char current_path[128]; // Для хранения человекочитаемого пути
extern char history[HISTORY_SIZE][BUF_SIZE];
extern int history_count; 
extern int history_index; // -1: текущая команда, 0..HISTORY_SIZE-1: индекс истории

extern "C" {
    void shell_print_prompt();
    void shell_clear_line();
    void shell_redraw();
    void shell_insert_char(char c);
    void shell_delete_char();
    void shell_add_to_history(const char* cmd);
    void shell_load_history(int index);
    void process_command(); // Встроенная функция обработки команд
    void fat_format_disk();
}

#endif // SHELL_H