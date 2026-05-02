#include "cmd_clear.h"
#include "../../kernel/screen.h"
#include "../../shell/shell.h"
#include "../../kernel/Scrollback.h"

void cmd_clear() {
    uint8_t attr = get_theme_color();
    uint16_t blank = (uint16_t)(attr << 8) | ' ';

    for (int row = 1; row < 25; row++) {
        for (int col = 0; col < 80; col++) {
            video_memory[row * 80 + col] = blank;
        }
    }
    scrollback_reset();
    cursor_pos = 160; 
    shell_draw_status_bar();
}