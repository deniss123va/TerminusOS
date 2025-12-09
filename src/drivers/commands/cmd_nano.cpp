#include "cmd_nano.h"
#include "../../kernel/screen.h" 
#include "../../kernel/keyboard.h" 
#include "../../fs/fat16.h" 
#include "../../lib/string.h" 

// Глобальное состояние для Nano-Lite
#define EDITOR_MAX_SIZE 1024
char editor_buffer[EDITOR_MAX_SIZE] = {0};
int editor_cursor_x = 0;
int editor_cursor_y = 0;
int editor_text_len = 0;

void buffer_to_coords(int index, int* x, int* y) {
    *x = 0;
    *y = 0;
    for (int i = 0; i < index; i++) {
        if (editor_buffer[i] == '\n') { *y += 1; *x = 0; } 
        else { *x += 1; }
        if (*x >= 80) { *y += 1; *x = 0; }
    }
}

int coords_to_buffer(int x, int y) {
    int index = 0;
    int current_x = 0;
    int current_y = 0;

    while (index < editor_text_len) {
        if (current_y == y && current_x == x) return index;
        
        char c = editor_buffer[index++];
        if (c == '\n') { current_y++; current_x = 0; } 
        else { 
            current_x++; 
            if (current_x >= 80) { current_y++; current_x = 0; } 
        }
        if (current_y > y) return index - 1; 
    }
    return editor_text_len;
}

void redraw_editor() {
    // Очистка экрана
    for(int i=0;i<80*25;i++) video_memory[i]=0x0F00;
    
    int screen_y = 0;
    int screen_x = 0;
    
    // Отрисовка текста из буфера
    for (int i = 0; i < editor_text_len && screen_y < 24; i++) {
        char c = editor_buffer[i];
        
        if (c == '\n') {
            screen_y++;
            screen_x = 0;
        } else {
            video_memory[screen_y * 80 + screen_x] = c | 0x0F00;
            screen_x++;
            if (screen_x >= 80) { 
                screen_y++;
                screen_x = 0;
            }
        }
    }

    // Статусная строка
    int status_pos = 24 * 80;
    const char* status = "MyOS Nano-lite. ESC to exit. Commands: create <file>";
    for (int i = 0; status[i] && i < 80; i++) {
        video_memory[status_pos + i] = status[i] | 0x4F00; 
    }

    update_vga_cursor(editor_cursor_x, editor_cursor_y);
}


void cmd_nano() {
    update_vga_cursor(0, 25); 

    // Сброс буфера
    for(int i=0; i<EDITOR_MAX_SIZE; i++) editor_buffer[i] = 0;
    editor_text_len = 0;
    editor_cursor_x = 0;
    editor_cursor_y = 0;
    
    redraw_editor();
    
    bool running = true;
    while(running) {
        char c = get_key(); 
        if (c == 0) continue;
        
        int current_pos_index = coords_to_buffer(editor_cursor_x, editor_cursor_y);
        
        switch(c) {
            case CHAR_ESC: // ESC для выхода
                running = false;
                break;
            case CHAR_ARROW_UP:
                if (editor_cursor_y > 0) editor_cursor_y--;
                break;
            case CHAR_ARROW_DOWN:
                if (editor_cursor_y < 23) editor_cursor_y++; 
                break;
            case CHAR_ARROW_LEFT:
                if (editor_cursor_x > 0) editor_cursor_x--;
                break;
            case CHAR_ARROW_RIGHT:
                if (editor_cursor_x < 79) editor_cursor_x++; 
                break;
            case '\b': // Backspace
                if (current_pos_index > 0) {
                    for (int i = current_pos_index - 1; i < editor_text_len; i++) {
                        editor_buffer[i] = editor_buffer[i+1];
                    }
                    editor_text_len--;
                    // Пересчет курсора
                    buffer_to_coords(current_pos_index - 1, &editor_cursor_x, &editor_cursor_y);
                }
                break;
            case '\n': // Enter
                if (editor_text_len < EDITOR_MAX_SIZE - 1) {
                    for (int i = editor_text_len; i >= current_pos_index; i--) {
                        editor_buffer[i+1] = editor_buffer[i];
                    }
                    editor_buffer[current_pos_index] = '\n';
                    editor_text_len++;
                    
                    editor_cursor_y++;
                    editor_cursor_x = 0;
                }
                break;
            default:
                if (c >= 32 && editor_text_len < EDITOR_MAX_SIZE - 1) { 
                    for (int i = editor_text_len; i >= current_pos_index; i--) {
                        editor_buffer[i+1] = editor_buffer[i];
                    }
                    editor_buffer[current_pos_index] = c;
                    editor_text_len++;
                    editor_cursor_x++;
                }
                break;
        }
        
        // Корректировка курсора после действия
        if (c != '\b') {
            buffer_to_coords(coords_to_buffer(editor_cursor_x, editor_cursor_y), &editor_cursor_x, &editor_cursor_y);
        }
        
        redraw_editor();
    }
    
    // Возврат в консольный режим
    for(int i=0;i<80*25;i++) video_memory[i]=0x0F00;
    cursor_pos = 0;
    update_vga_cursor(cursor_pos % 80, cursor_pos / 80);
    println("Exited editor. Use 'create <file>' to save content.");
}

void cmd_create(char* filename) {
    if (editor_text_len == 0) {
        println("Editor buffer is empty. Nothing to save.");
        return;
    }
    
    if (fat_create_file(filename, editor_buffer, editor_text_len)) {
        println("File saved successfully.");
    } else {
        println("File creation failed.");
    }
}