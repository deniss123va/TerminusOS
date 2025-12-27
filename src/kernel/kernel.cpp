#include <stdint.h>
#include <stdbool.h> 

#include "screen.h"     // Для VGA/I/O
#include "keyboard.h"   // Для get_key, клавиатурных констант
#include "../shell/shell.h" // Для shell_print_prompt, shell_*
#include "../fs/fat16.h"  // Для FAT16_ROOT_DIR_CLUSTER
#include "../drivers/disk.h" // ДОБАВИТЬ: для ata_read_sector и sector_buffer

// **********************************************
// ===== ГЛОБАЛЬНЫЕ ОБЪЯВЛЕНИЯ =====
// **********************************************

// Выносим объявление panic в глобальную область видимости, 
// чтобы компилятор применил к нему C-связывание.
extern "C" {
    void panic(const char* message);
}

// **********************************************
// ===== main =====
// **********************************************

extern "C" void kmain(){
    // Очистка и приветствие
    // ОШИБКА В ТЕКСТЕ: "tihs" -> "this"
    for(int i=0;i<80*25;i++) video_memory[i]=0x0F00;
    fat_init();
    println("Welcome to TerminusOS 0.2,5! this is minimal OS");
    println("Type 'help' for commands.");
    
    
    // Установка курсора в начало строки и вывод приглашения
    cursor_pos = 2 * 80; 
    update_vga_cursor(cursor_pos % 80, cursor_pos / 80);
    shell_print_prompt(); 

    while(1){
        // Блок extern "C" для panic УДАЛЕН отсюда
        
        char c = get_key();
        if(c==0) continue;

        switch(c) {
            case '\n': // Enter
                print_char('\n');
                process_command();
                shell_print_prompt(); // Вывод нового приглашения
                break;
            case '\b': // Backspace
                shell_delete_char();
                break;
            case CHAR_ARROW_LEFT: // Стрелка влево
                if (cursor_offset > 0) cursor_offset--;
                shell_redraw();
                break;
            case CHAR_ARROW_RIGHT: // Стрелка вправо
                if (cursor_offset < buf_len) cursor_offset++;
                shell_redraw();
                break;
            case CHAR_ARROW_UP: // Стрелка вверх (История)
                if (history_index < history_count - 1) {
                    shell_load_history(history_index + 1);
                }
                break;
            case CHAR_ARROW_DOWN: // Стрелка вниз (История)
                if (history_index > -1) {
                    shell_load_history(history_index - 1);
                }
                break;
            default:
                if (c >= 32) { // Печатаемые символы
                    shell_insert_char(c);
                }
                break;
        }
    }
}