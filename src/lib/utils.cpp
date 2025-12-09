#include "utils.h"

// Укорачивает строку dest до последнего '/' (для cd ..)
void dirname(char* dest) {
    int len = strlen(dest);
    if (len <= 1) return; 

    int last_slash = len - 1;
    while (last_slash > 0 && dest[last_slash] != '/') {
        last_slash--;
    }

    if (last_slash > 0) {
        dest[last_slash] = 0;
    } else {
        strcpy(dest, "/"); 
    }
}

// Вспомогательная функция для извлечения аргумента (часть команды)
char* get_arg(char* command, char* output_buffer) {
    int start = 0;
    while(command[start] != ' ' && command[start] != 0) start++;
    if (command[start] == 0) return 0;
    
    start++;
    int i = 0;
    while(command[start + i] != 0) {
        output_buffer[i] = command[start + i];
        i++;
    }
    output_buffer[i] = 0;
    return output_buffer;
}