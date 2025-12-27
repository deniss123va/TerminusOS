#include <stdint.h>
#include <stddef.h>

#include "screen.h"
#include "keyboard.h"
#include "../shell/shell.h"
#include "../fs/fat16.h"
#include "../drivers/disk.h"

extern "C" {
    void panic(const char* message);
}

extern "C" void kmain(){
    fat_init();
    
    disable_vga_cursor();  // ✅ Отключаем мигающий курсор
    
    for(int i=0;i<80*25;i++) video_memory[i]=0x0F00;
    println("Welcome to TerminusOS 0.3.0! this is minimal OS");
    println("Type 'help' for commands.");
    
    cursor_pos = 2 * 80;
    update_vga_cursor(cursor_pos % 80, cursor_pos / 80);
    
    shell_print_prompt();
    shell_init_cursor();  // ✅ Инициализация курсора
    
    while(1){
        char c = get_key();
        if(c==0) continue;
        
        switch(c) {
            case '\n': // Enter
                clear_block_cursor(last_cursor_x, last_cursor_y);  // ✅ Очищаем курсор
                print_char('\n');
                process_command();
                shell_redraw();
                break;
                
            case '\b': // Backspace
                shell_delete_char();
                break;
                
            case CHAR_ARROW_LEFT:
                if (cursor_offset > 0) cursor_offset--;
                shell_redraw();
                break;
                
            case CHAR_ARROW_RIGHT:
                if (cursor_offset < buf_len) cursor_offset++;
                shell_redraw();
                break;
                
            case CHAR_ARROW_UP:
                if (history_index < history_count - 1) {
                    shell_load_history(history_index + 1);
                }
                break;
                
            case CHAR_ARROW_DOWN:
                if (history_index > -1) {
                    shell_load_history(history_index - 1);
                }
                break;
                
            default:
                if (c >= 32) {
                    shell_insert_char(c);
                }
                break;
        }
    }
}
