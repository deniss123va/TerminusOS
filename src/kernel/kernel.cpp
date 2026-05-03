#include <stdint.h>
#include <stdbool.h>
#include "../lib/screen.h"
#include "keyboard.h"
#include "../shell/shell.h"
#include "../drivers/fat32.h"
#include "../drivers/disk.h"
#include "../drivers/rtc.h"
#include "../lib/config.h"
#include "../commands/cmd_uptime.h"

extern "C" {
    void panic(const char* message);
}


extern "C" void kmain(){
    // Инициализация FAT
    fat32_init();
    
    // Инициализация RTC
    rtc_init();
    
    // Отключаем мигающий курсор
    disable_vga_cursor();
    
    // Очищаем экран
    for(int i=0;i<80*25;i++) video_memory[i]=0x0F00;
    
    config_load();
    shell_load_history_file();   // восстанавливаем историю команд с диска
    uint8_t attr = get_theme_color();
    uint16_t blank = (attr << 8) | ' ';
    for(int i=0; i<80*25; i++) video_memory[i]=blank;

    // Рисуем статус-бар
    shell_update_time();
    shell_draw_status_bar();
    
    // Выводим приветствие
    cursor_pos = 80;  // ✅ Строка 1 (80 символов = 80/80 = строка 1)
    println("Welcome to TerminusOS 0.4.3!");
    println("Type 'help' for commands.");
    // После двух println курсор будет на строке 3 автоматически
    
    shell_print_prompt();
    shell_init_cursor();
    
    // Счетчик для обновления времени
    static int tick_counter = 0;
    
    while(1){
        uint8_t c = get_key();
        
        // Обновляем время каждые 50 итераций
        if(++tick_counter > 50) {
            shell_update_time();
            tick_counter = 0;
        }
        
        if(c==0) continue;
        cmd_uptime_tick();
        switch(c) {
            case CHAR_PGUP:
                shell_handle_pgup();
                break;
            case CHAR_PGDN:
                shell_handle_pgdn();
                break;
            case '\n':
                if (shell_is_in_scrollback()) { shell_exit_scrollback(); break; }
                clear_block_cursor(last_cursor_x, last_cursor_y);
                tab_reset();
                history_index = -1;
                print_char('\n');
                process_command();
                shell_print_prompt();
                shell_init_cursor();
                break;

            case CHAR_TAB:
                shell_handle_tab(false);
                break;

            case CHAR_SHIFT_TAB:
                shell_handle_tab(true);
                break;

            case CHAR_DEL: {
                // Delete — удалить символ под курсором (справа)
                if (cursor_offset < buf_len) {
                    for (int i = cursor_offset; i < buf_len; i++) buffer[i] = buffer[i+1];
                    buf_len--;
                    buffer[buf_len] = 0;
                    shell_redraw();
                }
                break;
            }

            case CHAR_CTRL_C:
                // Очистить буфер ввода
                clear_block_cursor(last_cursor_x, last_cursor_y);
                buf_len = 0;
                cursor_offset = 0;
                buffer[0] = 0;
                tab_reset();
                print_char('\n');
                shell_print_prompt();
                shell_init_cursor();
                break;

            case CHAR_CTRL_A:
            case CHAR_HOME:
                cursor_offset = 0;
                shell_redraw();
                break;

            case CHAR_CTRL_E:
            case CHAR_END:
                cursor_offset = buf_len;
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
                if (cursor_offset < buf_len) {
                    cursor_offset++;
                    shell_redraw();
                } else {
                    // курсор в конце — принять всю подсказку
                    shell_tab_accept();
                }
                break;

            case CHAR_SHIFT_RIGHT:
                if (shell_has_suggestion()) {
                    shell_tab_accept_one();   // принять 1 символ подсказки
                } else if (cursor_offset < buf_len) {
                    cursor_offset++;
                    shell_redraw();
                }
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
                    if (shell_is_in_scrollback()) shell_exit_scrollback();
                    shell_insert_char(c);
                }
                break;
        }
    }
}