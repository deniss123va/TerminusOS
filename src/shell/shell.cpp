#include "shell.h"
#include "../kernel/screen.h"
#include "../lib/string.h"
#include "../lib/utils.h"
#include "builtin.h"

// Глобальные переменные Shell
char buffer[BUF_SIZE];
int buf_len = 0;
int cursor_offset = 0;

char current_path[128] = "/";
char history[HISTORY_SIZE][BUF_SIZE] = {0};
int history_count = 0; 
int history_index = -1;

// Для отслеживания позиции курсора
int last_cursor_x = 0;
int last_cursor_y = 0;

// ==============================================
// Очистка строки
// ==============================================

void shell_clear_line() {
    int current_row = cursor_pos / 80;
    int start_pos = current_row * 80;
    
    // Очищаем строку
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

// ✅ НОВАЯ ФУНКЦИЯ: Инициализация курсора
void shell_init_cursor() {
    int prompt_len = strlen(current_path) + 2;
    last_cursor_x = prompt_len;
    last_cursor_y = cursor_pos / 80;
    draw_block_cursor(last_cursor_x, last_cursor_y);
}


// ==============================================
// Перерисовка с видимым курсором
// ==============================================

void shell_redraw() {
    // Убираем старый курсор
    clear_block_cursor(last_cursor_x, last_cursor_y);
    
    // Очищаем строку
    shell_clear_line();
    
    // Рисуем приглашение
    shell_print_prompt();
    
    // Терминируем буфер
    buffer[buf_len] = 0;
    
    // Выводим команду
    print(buffer);
    
    // Вычисляем позицию курсора
    int prompt_len = strlen(current_path) + 2; 
    int current_row = cursor_pos / 80;
    int new_x = prompt_len + cursor_offset;
    
    // Сохраняем позицию
    last_cursor_x = new_x;
    last_cursor_y = current_row;
    
    // Рисуем видимый курсор
    draw_block_cursor(new_x, current_row);
    
    // Обновляем аппаратный курсор
    update_vga_cursor(new_x, current_row);
    
    // Обновляем cursor_pos
    cursor_pos = current_row * 80 + new_x;
}

// ==============================================
// Вставка символа
// ==============================================

void shell_insert_char(char c) {
    if (buf_len >= BUF_SIZE - 1) return;

    // Сдвигаем символы вправо
    for (int i = buf_len; i > cursor_offset; i--) {
        buffer[i] = buffer[i-1];
    }
    
    buffer[cursor_offset] = c;
    buf_len++;
    buffer[buf_len] = 0;
    cursor_offset++;
    
    shell_redraw();
}

// ==============================================
// Удаление символа (Backspace)
// ==============================================

void shell_delete_char() {
    if (cursor_offset == 0) return;

    // Сдвигаем символы влево
    for (int i = cursor_offset - 1; i < buf_len; i++) {
        buffer[i] = buffer[i+1];
    }
    
    buf_len--;
    cursor_offset--;
    buffer[buf_len] = 0;
    
    shell_redraw();
}

// ==============================================
// История команд
// ==============================================

void shell_add_to_history(const char* cmd) {
    if (strlen(cmd) == 0) return;
    
    if (history_count > 0 && strcmp(history[HISTORY_SIZE - 1], cmd) == 0) {
        return;
    }

    for (int i = 0; i < HISTORY_SIZE - 1; i++) {
        strcpy(history[i], history[i + 1]);
    }
    
    strcpy(history[HISTORY_SIZE - 1], cmd);
    
    if (history_count < HISTORY_SIZE) {
        history_count++;
    }
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

// ==============================================
// Обработка команд
// ==============================================

void process_command(){
    buffer[buf_len] = 0;
    
    shell_add_to_history(buffer);
    history_index = -1;
    
    // Новая строка
    print_char('\n');
    
    // ========== КОМАНДЫ ==========
    
    if(strcmp(buffer,"help")==0){
        println("Available commands:");
        println("  help        - Show this help");
        println("  clear       - Clear screen");
        println("  ls          - List files in current directory");
        println("  cat <file>  - Show file contents");
        println("  pwd         - Show current directory");
        println("  cd <dir>    - Change directory");
        println("  mkdir <dir> - Create directory");
        println("  rm <name>   - Delete file or directory");
        println("  mv <a> <b>  - Rename/move file");
        println("  create <f>  - Create empty file");
        println("  nano <file> - Text editor (create/edit)");
        println("  fatcheck    - Check FAT table");
        println("  fsd         - Format disk (WARNING!)");
        println("  info        - System information");
        println("  exit        - Halt system");
    }
    else if(strcmp(buffer,"clear")==0){
        for(int i=0;i<80*25;i++) video_memory[i]=0x0F00;
        cursor_pos=0;
    }
    else if(strncmp(buffer,"echo ",5)==0){
        println(buffer+5);
    }
    else if(strcmp(buffer,"fatcheck")==0){
        cmd_fat_check();
    }
    else if(strcmp(buffer,"ls")==0){
        cmd_ls_disk();
    }
    else if(strncmp(buffer,"cat ",4)==0){
        cmd_disk_cat(buffer + 4);
    }
    else if(strcmp(buffer,"pwd")==0){ 
        cmd_pwd();
    }
    else if(strncmp(buffer,"cd ",3)==0){ 
        cmd_cd(buffer+3);
    }
    else if(strncmp(buffer,"mkdir ",6)==0){ 
        cmd_mkdir(buffer+6);
    }
    else if(strncmp(buffer,"rm ",3)==0){ 
        cmd_rm(buffer+3);
    }
    else if(strncmp(buffer,"mv ",3)==0){ 
        cmd_mv(buffer+3);
    }
    else if(strcmp(buffer,"nano")==0){
        println("Usage: nano <filename>");
    }
    else if(strncmp(buffer,"nano ",5)==0){
        cmd_nano(buffer + 5);
    }
    else if(strncmp(buffer,"edit ",5)==0){
        cmd_nano(buffer + 5);  // ✅ edit теперь вызывает nano
    }
    else if(strcmp(buffer,"info")==0){ 
        cmd_info();
    }
    else if(strncmp(buffer,"create ", 7) == 0){ 
        cmd_create(buffer + 7);
    }
    else if(strcmp(buffer,"read")==0){ 
        cmd_read_disk();
    }
    else if(strcmp(buffer,"fsd")==0){ 
        fat_format_disk();
    }
    else if(strcmp(buffer,"exit")==0){
        println("Halting...");
        while(1){__asm__ __volatile__("hlt");}
    }
    else if(strlen(buffer) > 0) {
        println("Unknown command");
    }
    
    // Полная очистка буфера
    for (int i = 0; i < BUF_SIZE; i++) buffer[i] = 0;
    buf_len = 0;
    cursor_offset = 0;
}
