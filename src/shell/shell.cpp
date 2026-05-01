#include "shell.h"
#include "../kernel/screen.h"
#include "../lib/string.h"
#include "../lib/utils.h"
#include "../drivers/rtc.h"
#include "../fs/fat32.h"
#include "builtin.h"

// Объявление внешней функции, которой нет в builtin.h
void cmd_theme(char* name);

// Глобальные переменные Shell
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

// Tab completion state
#define TAB_MAX_MATCHES 16
static char   tab_matches[TAB_MAX_MATCHES][128];
static int    tab_match_count = 0;
static int    tab_match_index = -1;   // -1 = нет активного ghost
static char   tab_prefix[128] = {0};  // слово перед курсором
static int    tab_prefix_start = 0;   // позиция начала prefix в buffer

void shell_init_status_bar() {
    strcpy(last_executed_command, "");
    strcpy(time_string, "--:--:--");
    status_bar_dirty = true;
    shell_draw_status_bar();
}

void shell_draw_status_bar() {
    uint16_t bar_attr = (theme_bar_bg << 4) | theme_bar_fg;
    uint16_t bar_val = bar_attr << 8;

    // Заливаем статус-бар пробелами
    for (int i = 0; i < 80; i++) {
        video_memory[0 * 80 + i] = bar_val | ' ';
    }

    // PATH
    int pos = 1;
    int path_len = strlen(current_path);
    if (path_len > 25) {
        path_len = 25;
    }
    for (int i = 0; i < path_len; i++) {
        video_memory[0 * 80 + pos + i] = bar_val | current_path[i];
    }
    pos += path_len;

    // CMD
    const char* cmd_label = " | CMD:";
    int cmd_label_len = strlen(cmd_label);
    for (int i = 0; i < cmd_label_len; i++) {
        video_memory[0 * 80 + pos + i] = bar_val | cmd_label[i];
    }
    pos += cmd_label_len;

    int cmd_len = strlen(last_executed_command);
    if (cmd_len > 28) {
        cmd_len = 28;
    }
    for (int i = 0; i < cmd_len; i++) {
        video_memory[0 * 80 + pos + i] = bar_val | last_executed_command[i];
    }

    // TIME
    int right_pos = 68;
    const char* time_label = "T:";
    for (int i = 0; time_label[i]; i++) {
        video_memory[0 * 80 + right_pos + i] = bar_val | time_label[i];
    }
    right_pos += 2;

    int time_len = strlen(time_string);
    if (time_len > 8) {
        time_len = 8;
    }
    for (int i = 0; i < time_len; i++) {
        video_memory[0 * 80 + right_pos + i] = bar_val | time_string[i];
    }

    status_bar_dirty = false;
}

void shell_update_time() {
    rtc_get_time_string(time_string);
    
    uint16_t bar_attr = (theme_bar_bg << 4) | theme_bar_fg;
    uint16_t bar_val = bar_attr << 8;

    int time_pos = 70;
    for (int i = 0; i < 8 && time_string[i]; i++) {
        video_memory[0 * 80 + time_pos + i] = bar_val | time_string[i];
    }
}

void shell_save_executed_command(const char* cmd) {
    strcpy(last_executed_command, cmd);
    shell_draw_status_bar();
}

// Shell функции

void shell_clear_line() {
    int current_row = cursor_pos / 80;
    if (current_row <= 1) current_row = 2; // Защита статус-бара
    
    uint8_t attr = get_theme_color();
    uint16_t blank = (attr << 8) | ' ';
    
    int start_pos = current_row * 80;
    for (int i = 0; i < 80; i++) {
        video_memory[start_pos + i] = blank;
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

void tab_reset() {
    tab_match_count = 0;
    tab_match_index = -1;
    tab_prefix[0]   = 0;
}

void shell_redraw() {
    clear_block_cursor(last_cursor_x, last_cursor_y);
    
    shell_draw_status_bar();
    
    shell_clear_line();
    shell_print_prompt();
    
    buffer[buf_len] = 0;
    print(buffer);
    
    int prompt_len = strlen(current_path) + 2;
    int current_row = cursor_pos / 80;
    int new_x = prompt_len + cursor_offset;

    // Рисуем ghost-текст (серым) если есть активное совпадение
    if (tab_match_index >= 0 && tab_match_index < tab_match_count) {
        const char* full  = tab_matches[tab_match_index];
        const char* ghost = full + strlen(tab_prefix); // суффикс после того, что уже напечатано
        int ghost_x = prompt_len + buf_len;
        uint8_t gray_attr = 0x08; // тёмно-серый на чёрном
        uint16_t gray_val  = (gray_attr << 8);
        for (int i = 0; ghost[i] && (ghost_x + i) < 80; i++) {
            video_memory[current_row * 80 + ghost_x + i] = gray_val | (uint8_t)ghost[i];
        }
    }

    last_cursor_x = new_x;
    last_cursor_y = current_row;
    draw_block_cursor(new_x, current_row);
    update_vga_cursor(new_x, current_row);
    cursor_pos = current_row * 80 + new_x;
}

void shell_insert_char(char c) {
    if (buf_len >= BUF_SIZE - 1) return;
    tab_reset();
    
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
    tab_reset();
    
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

// Находит последнее слово в буфере (слово после последнего пробела)
static void get_last_word(char* out, int* start_out) {
    int start = 0;
    for (int i = 0; i < cursor_offset; i++)
        if (buffer[i] == ' ') start = i + 1;
    int i = 0;
    while (start + i < cursor_offset) { out[i] = buffer[start + i]; i++; }
    out[i] = 0;
    *start_out = start;
}

void shell_handle_tab(bool reverse) {
    // Если уже есть список — просто листаем
    if (tab_match_count > 0) {
        if (!reverse)
            tab_match_index = (tab_match_index + 1) % tab_match_count;
        else
            tab_match_index = (tab_match_index - 1 + tab_match_count) % tab_match_count;
        shell_redraw();
        return;
    }

    // Первый Tab — собираем совпадения
    char prefix[128] = {0};
    int  pstart = 0;
    get_last_word(prefix, &pstart);

    // Получаем список файлов из FAT32
    tab_match_count = fat32_tab_complete(prefix, tab_matches, TAB_MAX_MATCHES);
    if (tab_match_count == 0) return;

    // Сохраняем prefix и позицию для последующих нажатий
    int k = 0; while (prefix[k]) { tab_prefix[k] = prefix[k]; k++; } tab_prefix[k] = 0;
    tab_prefix_start = pstart;
    tab_match_index  = reverse ? tab_match_count - 1 : 0;
    shell_redraw();
}

// Принять текущее ghost-дополнение (вписать суффикс в буфер)
void shell_tab_accept() {
    if (tab_match_index < 0 || tab_match_index >= tab_match_count) return;
    const char* full   = tab_matches[tab_match_index];
    const char* suffix = full + strlen(tab_prefix);
    for (int i = 0; suffix[i]; i++) {
        if (buf_len < BUF_SIZE - 1) {
            // вставляем в позицию cursor_offset
            for (int j = buf_len; j > cursor_offset; j--) buffer[j] = buffer[j-1];
            buffer[cursor_offset++] = suffix[i];
            buf_len++;
        }
    }
    buffer[buf_len] = 0;
    tab_reset();
    shell_redraw();
}

void process_command() {
    buffer[buf_len] = 0;
    shell_add_to_history(buffer);
    shell_save_executed_command(buffer);
    print_char('\n');
    
    if (strcmp(buffer, "clear") == 0) {
        cmd_clear();
    }
    else if (strcmp(buffer, "help") == 0) {
        cmd_help();
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
    else if (strncmp(buffer, "settings", 8) == 0) {
        const char* args = (buffer[8] == ' ') ? buffer + 9 : buffer + 8;
        cmd_settings(args);
    }
    else if (strncmp(buffer, "rm ", 3) == 0) {
        cmd_rm(buffer + 3);
    }
    else if (strncmp(buffer, "mv ", 3) == 0) {
        cmd_mv(buffer + 3);
    }
    else if (strcmp(buffer, "cp") == 0) {
        println("Usage: cp <source> <dest>");
    }
    else if (strncmp(buffer, "cp ", 3) == 0) {
        cmd_cp(buffer + 3);
    }
    else if (strcmp(buffer, "theme") == 0) {
        cmd_theme((char*)"");
    }
    else if (strncmp(buffer, "theme ", 6) == 0) {
        cmd_theme(buffer + 6);
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
    else if (strcmp(buffer, "reboot") == 0) {
        cmd_reboot();
    }
    else if (strcmp(buffer, "shutdown") == 0) {
        cmd_shutdown();
    }
    else if (strcmp(buffer, "exit") == 0) {
        cmd_shutdown();
    }
    else if (strlen(buffer) > 0) {
        println("Unknown command");
    }
    
    for (int i = 0; i < BUF_SIZE; i++) buffer[i] = 0;
    buf_len = 0;
    cursor_offset = 0;
}