#include "shell.h"

#include "../kernel/screen.h"
#include "../lib/string.h"
#include "../lib/utils.h"
#include "../drivers/rtc.h"
#include "builtin.h"

// ==============================================
// Глобальные переменные Shell
// ==============================================
char buffer[BUF_SIZE];
int buf_len = 0;
int cursor_offset = 0;
char current_path[128] = "/";
char history[HISTORY_SIZE][BUF_SIZE] = {0};
int history_count = 0;
int history_index = -1;
int last_cursor_x = 0;
int last_cursor_y = 0;

static char last_executed_command[BUF_SIZE] = {0};
static char time_string[16] = "--:--:--";
static bool status_bar_dirty = true;

void shell_init_status_bar() {
    strcpy(last_executed_command, "");
    strcpy(time_string, "--:--:--");
    status_bar_dirty = true;
    shell_draw_status_bar();
}

void shell_draw_status_bar() {
    // 1. Заливаем статус-бар БЕЛЫМ фоном (0x70 = белый фон, черный текст)
    for (int i = 0; i < 80; i++) {
        video_memory[0 * 80 + i] = 0x7000 | ' ';
    }
    
    // 2. Левая часть: PATH (белый фон, черный текст)
    int pos = 1;
    int path_len = strlen(current_path);
    if (path_len > 25) path_len = 25;
    for (int i = 0; i < path_len; i++) {
        video_memory[0 * 80 + pos + i] = 0x7000 | current_path[i];
    }
    
    pos += path_len;
    const char* cmd_label = " | CMD:";
    for (int i = 0; cmd_label[i]; i++) {
        video_memory[0 * 80 + pos + i] = 0x7000 | cmd_label[i];
    }
    
    pos += strlen(cmd_label);
    int cmd_len = strlen(last_executed_command);
    if (cmd_len > 28) cmd_len = 28;
    for (int i = 0; i < cmd_len; i++) {
        video_memory[0 * 80 + pos + i] = 0x7000 | last_executed_command[i];
    }
    
    // 3. Правая часть: TIME (белый фон, черный текст)
    int right_pos = 68;
    const char* time_label = "T:";
    for (int i = 0; time_label[i]; i++) {
        video_memory[0 * 80 + right_pos + i] = 0x7000 | time_label[i];
    }
    
    right_pos += 2;
    for (int i = 0; i < 8 && time_string[i]; i++) {
        video_memory[0 * 80 + right_pos + i] = 0x7000 | time_string[i];
    }
    
    status_bar_dirty = false;
}

// 🔥 Быстрое обновление только времени
void shell_update_time() {
    rtc_get_time_string(time_string);
    int time_pos = 70;
    for (int i = 0; i < 8 && time_string[i]; i++) {
        video_memory[0 * 80 + time_pos + i] = 0x7000 | time_string[i];  // Белый фон, черный текст
    }
}

// 🔥 Сохранить выполненную команду
void shell_save_executed_command(const char* cmd) {
    strcpy(last_executed_command, cmd);
    shell_draw_status_bar();
}

// ==============================================
// Shell функции
// ==============================================

void shell_clear_line() {
    int current_row = cursor_pos / 80;
    if (current_row <= 1) current_row = 2; // Защита статус-бара
    
    int start_pos = current_row * 80;
    for (int i = 0; i < 80; i++) {
        video_memory[start_pos + i] = 0x0F00;
    }
    
    cursor_pos = start_pos;
    update_vga_cursor(cursor_pos % 80, cursor_pos / 80);
}

void shell_print_prompt() {
    print(current_path);
    print("$ ");
}

void shell_init_cursor() {
    int prompt_len = strlen(current_path) + 2;
    last_cursor_x = prompt_len;
    last_cursor_y = cursor_pos / 80;
    draw_block_cursor(last_cursor_x, last_cursor_y);
}

void shell_redraw() {
    clear_block_cursor(last_cursor_x, last_cursor_y);
    
    // 🔥 Всегда рисуем статус-бар
    shell_draw_status_bar();
    
    shell_clear_line();
    shell_print_prompt();
    
    buffer[buf_len] = 0;
    print(buffer);
    
    int prompt_len = strlen(current_path) + 2;
    int current_row = cursor_pos / 80;
    int new_x = prompt_len + cursor_offset;
    
    last_cursor_x = new_x;
    last_cursor_y = current_row;
    draw_block_cursor(new_x, current_row);
    update_vga_cursor(new_x, current_row);
    cursor_pos = current_row * 80 + new_x;
}

void shell_insert_char(char c) {
    if (buf_len >= BUF_SIZE - 1) return;
    
    for (int i = buf_len; i > cursor_offset; i--) {
        buffer[i] = buffer[i-1];
    }
    
    buffer[cursor_offset] = c;
    buf_len++;
    buffer[buf_len] = 0;
    cursor_offset++;
    
    shell_redraw();
}

void shell_delete_char() {
    if (cursor_offset == 0) return;
    
    for (int i = cursor_offset - 1; i < buf_len; i++) {
        buffer[i] = buffer[i+1];
    }
    
    buf_len--;
    cursor_offset--;
    buffer[buf_len] = 0;
    
    shell_redraw();
}

void shell_add_to_history(const char* cmd) {
    if (strlen(cmd) == 0) return;
    if (history_count > 0 && strcmp(history[HISTORY_SIZE - 1], cmd) == 0) return;
    
    for (int i = 0; i < HISTORY_SIZE - 1; i++) {
        strcpy(history[i], history[i + 1]);
    }
    
    strcpy(history[HISTORY_SIZE - 1], cmd);
    if (history_count < HISTORY_SIZE) history_count++;
}

void shell_load_history(int index) {
    if (index < 0) {
        for (int i = 0; i < BUF_SIZE; i++) buffer[i] = 0;
        buf_len = 0;
        history_index = -1;
    } else if (index < history_count) {
        int actual_index = HISTORY_SIZE - history_count + index;
        strcpy(buffer, history[actual_index]);
        buf_len = strlen(buffer);
        history_index = index;
    }
    
    cursor_offset = buf_len;
    shell_redraw();
}

void process_command() {
    buffer[buf_len] = 0;
    shell_add_to_history(buffer);
    shell_save_executed_command(buffer);
    print_char('\n');
    
    // ========== КОМАНДЫ ==========
    if(strcmp(buffer,"help")==0){
        println("Available commands:");
        println("  help     - Show this help");
        println("  clear    - Clear screen");
        println("  ls       - List files in current directory");
        println("  cat      - Show file contents");
        println("  date     - Show current date and time");
        println("  pwd      - Show current directory");
        println("  cd       - Change directory");
        println("  mkdir    - Create directory");
        println("  rm       - Delete file or directory");
        println("  mv       - Rename/move file");
        println("  create   - Create empty file");
        println("  nano     - Text editor (create/edit)");
        println("  fatcheck - Check FAT table");
        println("  fsd      - Format disk (WARNING!)");
        println("  info     - System information");
        println("  exit     - Halt system");
    }
    else if (strcmp(buffer, "clear") == 0) {
        // Очищаем только рабочие строки (1-24), сохраняем статус-бар
        for (int row = 1; row < 25; row++) {
            for (int col = 0; col < 80; col++) {
                video_memory[row * 80 + col] = 0x0F00;
            }
        }
        cursor_pos = 160; // Строка 2
        shell_draw_status_bar();
    }
    else if (strncmp(buffer, "echo ", 5) == 0) {
        println(buffer + 5);
    }
    else if (strcmp(buffer, "fatcheck") == 0) {
        cmd_fat_check();
    }
    else if (strcmp(buffer, "ls") == 0) {
        cmd_ls_disk();
    }
    else if (strncmp(buffer, "cat ", 4) == 0) {
        cmd_disk_cat(buffer + 4);
    }
    else if (strcmp(buffer, "pwd") == 0) {
        cmd_pwd();
    }
    else if (strncmp(buffer, "cd ", 3) == 0) {
        cmd_cd(buffer + 3);
    }
    else if (strncmp(buffer, "mkdir ", 6) == 0) {
        cmd_mkdir(buffer + 6);
    }
    else if (strncmp(buffer, "rm ", 3) == 0) {
        cmd_rm(buffer + 3);
    }
    else if (strncmp(buffer, "mv ", 3) == 0) {
        cmd_mv(buffer + 3);
    }
    else if (strcmp(buffer, "nano") == 0) {
        println("Usage: nano <filename>");
    }
    else if (strncmp(buffer, "nano ", 5) == 0) {
        cmd_nano(buffer + 5);
    }
    else if (strncmp(buffer, "edit ", 5) == 0) {
        cmd_nano(buffer + 5);
    }
    else if (strcmp(buffer, "info") == 0) {
        cmd_info();
    }
    else if (strncmp(buffer, "create ", 7) == 0) {
        cmd_create(buffer + 7);
    }
    else if (strcmp(buffer, "read") == 0) {
        cmd_read_disk();
    }
    else if (strcmp(buffer, "fsd") == 0) {
        fat_format_disk();
    }
    else if (strcmp(buffer, "date") == 0) {
    cmd_date();
    }
    else if (strcmp(buffer, "exit") == 0) {
        println("Halting...");
        while (1) { __asm__ __volatile__("hlt"); }
    }
    else if (strlen(buffer) > 0) {
        println("Unknown command");
    }
    
    // 🔥 ВОССТАНАВЛИВАЕМ ПОЗИЦИЮ
    for (int i = 0; i < BUF_SIZE; i++) buffer[i] = 0;
    buf_len = 0;
    cursor_offset = 0;
}