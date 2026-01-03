#include <stdint.h>
#include <stdbool.h>
#include "screen.h"
#include "keyboard.h"
#include "../shell/shell.h"
#include "../fs/fat16.h"
#include "../drivers/disk.h"
#include "../drivers/rtc.h"


extern "C" {
    void panic(const char* message);
}


extern "C" void kmain(){
    // Инициализация FAT
    fat_init();
    
    // Инициализация RTC
    rtc_init();
    
    // Отключаем мигающий курсор
    disable_vga_cursor();
    
    // Очищаем экран
    for(int i=0;i<80*25;i++) video_memory[i]=0x0F00;
    
    // Рисуем статус-бар
    shell_update_time();
    shell_draw_status_bar();
    
    // Выводим приветствие (начиная со строки 1)
    cursor_pos = 80;  // ✅ Строка 1 (80 символов = 80/80 = строка 1)
    println("Welcome to TerminusOS 0.3.5! this is minimal OS");
    println("Type 'help' for commands.");
    // После двух println курсор будет на строке 3 автоматически
    
    shell_print_prompt();
    shell_init_cursor();
    
    // Счетчик для обновления времени
    static int tick_counter = 0;
    
    while(1){
        char c = get_key();
        
        // Обновляем время каждые 50 итераций
        if(++tick_counter > 50) {
            shell_update_time();
            tick_counter = 0;
        }
        
        if(c==0) continue;
        
        switch(c) {
            case '\n':
                clear_block_cursor(last_cursor_x, last_cursor_y);
                history_index = -1;
                print_char('\n');
                process_command();  // Курсор остается там, где закончился вывод
                shell_print_prompt(); // Промпт рисуется на текущей позиции
                shell_init_cursor();
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
                // ✅ Перевернутая логика - идем от последней команды к первой
                if (history_index == -1) {
                    // Первое нажатие - показываем последнюю команду
                    if (history_count > 0) {
                        shell_load_history(history_count - 1);
                    }
                } else if (history_index > 0) {
                    // Идем к более старым командам
                    shell_load_history(history_index - 1);
                }
                break;
                
            case CHAR_ARROW_DOWN:
                // ✅ Идем к более новым командам
                if (history_index != -1 && history_index < history_count - 1) {
                    shell_load_history(history_index + 1);
                } else if (history_index == history_count - 1) {
                    // Если дошли до последней - очищаем буфер
                    shell_load_history(-1);
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