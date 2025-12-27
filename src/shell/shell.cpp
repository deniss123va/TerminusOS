#include "shell.h"
#include "../kernel/screen.h" // Для print, println, update_vga_cursor
#include "../lib/string.h" // Для strlen, strcmp, strcpy, strcat
#include "../lib/utils.h" // Для get_arg
#include "builtin.h" // Для cmd_ls_vfs и т.д.

// Глобальное состояние Shell и Курсор ввода
char buffer[BUF_SIZE];
int buf_len = 0; // Длина введенной строки
int cursor_offset = 0; // Смещение курсора внутри buffer (для редактирования)

char current_path[128] = "/"; // Для хранения человекочитаемого пути
char history[HISTORY_SIZE][BUF_SIZE] = {0};
int history_count = 0; 
int history_index = -1; // -1: текущая команда, 0..HISTORY_SIZE-1: индекс истории

// **********************************************
// ===== 10. Shell Line Editing (Ввод и Редактирование) =====
// **********************************************

// Очищает строку, начиная с текущей позиции курсора (для перерисовки)
void shell_clear_line() {
    int current_row = cursor_pos / 80;
    int start_pos = current_row * 80;
    
    // Очищаем всю строку
    for (int i = 0; i < 80; i++) {
        video_memory[start_pos + i] = 0x0F00;
    }
    
    // Восстанавливаем cursor_pos в начало строки для вывода приглашения
    cursor_pos = start_pos;
    update_vga_cursor(cursor_pos % 80, cursor_pos / 80);
}

void shell_print_prompt() {
    print(current_path);
    print("$ ");
}

// Полностью перерисовывает строку
void shell_redraw() {
    shell_clear_line();
    shell_print_prompt();
    
    // Терминируем строку перед выводом
    buffer[buf_len] = 0;
    print(buffer);
    
    // Установка курсора на нужную позицию
    int prompt_len = strlen(current_path) + 2; 
    int current_row = cursor_pos / 80;
    int new_x = prompt_len + cursor_offset;
    update_vga_cursor(new_x, current_row);
    
    // Обновление cursor_pos для следующей операции print_char
    cursor_pos = current_row * 80 + new_x;
}


void shell_insert_char(char c) {
    if (buf_len >= BUF_SIZE - 1) return; 

    // Сдвиг символов вправо для вставки
    for (int i = buf_len; i > cursor_offset; i--) {
        buffer[i] = buffer[i-1];
    }
    buffer[cursor_offset] = c;
    buf_len++;
    cursor_offset++;
    
    shell_redraw();
}

void shell_delete_char() {
    // Удаление символа перед курсором (Backspace)
    if (cursor_offset == 0) return; 

    // Сдвиг символов влево (удаление символа перед курсором)
    for (int i = cursor_offset - 1; i < buf_len; i++) {
        buffer[i] = buffer[i+1];
    }
    buf_len--;
    cursor_offset--;
    
    shell_redraw();
}

void shell_add_to_history(const char* cmd) {
    if (strlen(cmd) == 0) return;
    
    // Проверяем, не является ли последняя команда такой же
    if (history_count > 0 && strcmp(history[HISTORY_SIZE - 1], cmd) == 0) {
        return;
    }

    // Сдвиг истории
    for (int i = 0; i < HISTORY_SIZE - 1; i++) {
        strcpy(history[i], history[i + 1]);
    }
    // Добавление новой команды в конец
    strcpy(history[HISTORY_SIZE - 1], cmd);
    
    if (history_count < HISTORY_SIZE) {
        history_count++;
    }
}

void shell_load_history(int index) {
    if (index < 0) { // Текущая (пустая) команда
        buffer[0] = 0;
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

// **********************************************
// ===== 12. Обработка команд Shell =====
// **********************************************

void process_command(){
    buffer[buf_len]=0;
    
    // Добавляем команду в историю
    shell_add_to_history(buffer);
    history_index = -1;
    
    // Локальный буфер для аргументов
    char arg_buf[128] = {0}; 

    if(strcmp(buffer,"help")==0){
        println("Commands:");
        println("  help, clear, exit");
        println("  pwd, cd <dir>, mkdir <dir>, disk_ls, disk_cat <file>");
        println("  rm <item>, mv <old> <new>");
        println("  nano, create <file>");
    } else if(strcmp(buffer,"clear")==0){
        for(int i=0;i<80*25;i++) video_memory[i]=0x0F00;
        cursor_pos=0;
    } else if(strncmp(buffer,"echo ",5)==0){
        println(buffer+5);
    } else if(strcmp(buffer,"fatcheck")==0){
        cmd_fat_check();
    } else if(strcmp(buffer,"ls")==0){
        cmd_ls_disk();
    } else if(strncmp(buffer,"cat ",4)==0){
        cmd_disk_cat(buffer + 4);
    } else if(strcmp(buffer,"disk_ls")==0){ 
        cmd_ls_disk();
    } else if(strcmp(buffer,"pwd")==0){ 
        cmd_pwd();
    } else if(strncmp(buffer,"cd ",3)==0){ 
        cmd_cd(buffer+3);
    } else if(strncmp(buffer,"mkdir ",6)==0){ 
        cmd_mkdir(buffer+6);
    } else if(strncmp(buffer,"rm ",3)==0){ 
        cmd_rm(buffer+3);
    } else if(strncmp(buffer,"mv ",3)==0){ 
        cmd_mv(buffer+3);
    } else if(strcmp(buffer,"nano")==0){ 
        cmd_nano();
    } else if(strcmp(buffer,"info")==0){ 
        cmd_info();
    } else if(strncmp(buffer,"create ", 7) == 0){ 
        cmd_create(buffer + 7);
    } else if(strcmp(buffer,"read")==0){ 
        cmd_read_disk();
    } else if(strcmp(buffer,"fsd")==0){ 
        fat_format_disk();
    } else if(strcmp(buffer,"exit")==0){
        println("Halting...");
        while(1){__asm__ __volatile__("hlt");}
    } else {
        println("Unknown command");
    }
    for (int i = 0; i < BUF_SIZE; i++) {
        buffer[i] = 0;
    }
    buf_len = 0;
    cursor_offset = 0;
}