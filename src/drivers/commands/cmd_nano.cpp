#include "cmd_nano.h"
#include "../../kernel/screen.h"
#include "../../kernel/keyboard.h"
#include "../../fs/fat16.h"
#include "../../drivers/disk.h"
#include "../../lib/string.h"

// Константы редактора
#define EDITOR_MAX_LINES 20
#define EDITOR_LINE_WIDTH 79
#define STATUS_BAR_ROW 23
#define HELP_BAR_ROW 24

// Глобальные переменные редактора
static char editor_buffer[EDITOR_MAX_LINES][EDITOR_LINE_WIDTH + 1];
static int current_line = 0;
static int current_col = 0;
static int total_lines = 1;
static char current_filename[64] = {0};
static bool is_dirty = false;

// Для отслеживания курсора
static int last_cursor_x = 0;
static int last_cursor_y = 0;

// Внешние переменные и функции
extern uint8_t sector_buffer[512];
extern void draw_block_cursor(int x, int y);
extern void clear_block_cursor(int x, int y);

// Helper functions for direct VGA manipulation
void print_char_at(char c, int x, int y, uint8_t color) {
    int pos = y * 80 + x;
    video_memory[pos] = (color << 8) | c;
}

void print_at(const char* str, int x, int y, uint8_t color) {
    int pos = y * 80 + x;
    int i = 0;
    while(str[i]) {
        video_memory[pos++] = (color << 8) | str[i++];
    }
}

void clear_screen() {
    for(int i = 0; i < 80 * 25; i++) {
        video_memory[i] = 0x0F00;
    }
    cursor_pos = 0;
}

void set_cursor(int x, int y) {
    update_vga_cursor(x, y);
}

void editor_print_hex(uint32_t n) {
    const char *hex = "0123456789ABCDEF";
    char buffer[11];
    buffer[0] = '0';
    buffer[1] = 'x';
    for(int i = 0; i < 8; i++) {
        buffer[9-i] = hex[n & 0xF];
        n >>= 4;
    }
    buffer[10] = 0;
    print(buffer);
}

void editor_clear_screen() {
    clear_screen();
}

void editor_draw_status_bar(const char* filename) {
    for(int i = 0; i < 80; i++) {
        print_char_at(' ', i, STATUS_BAR_ROW, 0x70);
    }

    char status[80];
    int len = 0;
    const char* title = " NANO EDITOR ";
    while(title[len]) {
        status[len] = title[len];
        len++;
    }

    status[len++] = '-';
    status[len++] = ' ';
    int name_len = strlen(filename);
    for(int i = 0; i < name_len && len < 60; i++) {
        status[len++] = filename[i];
    }

    if(is_dirty) {
        status[len++] = ' ';
        status[len++] = '*';
    }

    status[len] = 0;
    for(int i = 0; i < len; i++) {
        print_char_at(status[i], i, STATUS_BAR_ROW, 0x70);
    }

    const char* help = "ESC Exit+Save  Arrows Navigate";
    for(int i = 0; i < 80; i++) {
        print_char_at(' ', i, HELP_BAR_ROW, 0x07);
    }

    int help_len = strlen(help);
    for(int i = 0; i < help_len; i++) {
        print_char_at(help[i], i, HELP_BAR_ROW, 0x07);
    }
}

void editor_redraw() {
    // Очищаем старый курсор
    clear_block_cursor(last_cursor_x, last_cursor_y);

    // Очищаем экран
    editor_clear_screen();

    // Рисуем содержимое
    for(int i = 0; i < total_lines; i++) {
        if(strlen(editor_buffer[i]) > 0) {
            print_at(editor_buffer[i], 0, i, 0x07);
        }
    }

    // Рисуем статус бар
    editor_draw_status_bar(current_filename);

    // Сохраняем позицию курсора
    last_cursor_x = current_col;
    last_cursor_y = current_line;

    // Рисуем видимый блочный курсор
    draw_block_cursor(current_col, current_line);

    // Обновляем аппаратный курсор
    set_cursor(current_col, current_line);
}

void editor_load_file(const char* filename) {
    println("Loading file...");
    FAT_FindResult result = fat_find_entry(filename, 0x00);
    if(!result.found) {
        println("New File");
        for(int i=0; i<1000000; i++);
        return;
    }

    uint32_t size = result.entry.file_size;
    uint16_t cluster = result.entry.start_cluster;
    if(size == 0) {
        println("Empty file");
        for(int i=0; i<1000000; i++);
        return;
    }

    static char file_content[EDITOR_MAX_LINES * (EDITOR_LINE_WIDTH + 1)];
    uint32_t bytes_read = 0;

    // Читаем файл кластер за кластером
    while(cluster < 0xFFF8 && bytes_read < size) {
        uint32_t lba = fat_cluster_to_lba(cluster);

        for(int sec = 0; sec < FAT_SECTORS_PER_CLUSTER && bytes_read < size; sec++) {
            ata_read_sector(lba + sec);

            for(int i = 0; i < 512 && bytes_read < size; i++) {
                file_content[bytes_read++] = sector_buffer[i];
            }
        }

        cluster = fat_get_next_cluster(cluster);
    }

    file_content[bytes_read] = 0;

    // Парсим построчно
    int line = 0;
    int col = 0;

    for(uint32_t i = 0; i < bytes_read && line < EDITOR_MAX_LINES; i++) {
        char c = file_content[i];

        if(c == '\n') {
            editor_buffer[line][col] = 0;
            line++;
            col = 0;
        } else if(c == '\r') {
            continue;
        } else if(col < EDITOR_LINE_WIDTH) {
            editor_buffer[line][col++] = c;
        }
    }

    if(col > 0 || line == 0) {
        editor_buffer[line][col] = 0;
        line++;
    }

    total_lines = line;

    print("Loaded ");
    editor_print_hex(bytes_read);
    print(" bytes, ");
    editor_print_hex(total_lines);
    println(" lines");
    for(int i=0; i<2000000; i++);
}

void editor_save_file() {
    if(strlen(current_filename) == 0) {
        for(int i=0; i<80; i++) print_char_at(' ', i, 22, 0x0C);
        print_at("ERROR: No filename!", 0, 22, 0x0C);
        return;
    }

    static char content[EDITOR_MAX_LINES * EDITOR_LINE_WIDTH];
    int pos = 0;
    for(int i = 0; i < total_lines; i++) {
        int len = strlen(editor_buffer[i]);
        for(int j = 0; j < len; j++) {
            content[pos++] = editor_buffer[i][j];
        }
        if(i < total_lines - 1) {
            content[pos++] = '\n';
        }
    }
    content[pos] = 0;

    editor_clear_screen();
    print("Saving ");
    print(current_filename);
    print("... Size: ");
    editor_print_hex(pos);
    println(" bytes");

    FAT_FindResult existing = fat_find_entry(current_filename, 0x00);
    if(existing.found) {
        print("Deleting old version... ");
        fat_delete_entry_data(current_filename, 0x00);
        println("Done.");
    }

    print("Creating new file... ");
    bool success = fat_create_file(current_filename, content, pos);
    if(success) {
        println("SUCCESS!");
        is_dirty = false;
    } else {
        println("FAILED!");
    }

    for(volatile int i=0; i<5000000; i++);
    editor_redraw();
}

void editor_insert_char(char c) {
    if(current_col >= EDITOR_LINE_WIDTH) return;
    int len = strlen(editor_buffer[current_line]);
    for(int i = len; i > current_col; i--) {
        editor_buffer[current_line][i] = editor_buffer[current_line][i-1];
    }
    editor_buffer[current_line][current_col] = c;
    editor_buffer[current_line][len+1] = 0;
    current_col++;
    is_dirty = true;
    editor_redraw();
}

void editor_new_line() {
    if(total_lines >= EDITOR_MAX_LINES) return;
    for(int i = total_lines; i > current_line + 1; i--) {
        strcpy(editor_buffer[i], editor_buffer[i-1]);
    }

    editor_buffer[current_line + 1][0] = 0;
    if(current_col < (int)strlen(editor_buffer[current_line])) {
        strcpy(editor_buffer[current_line + 1], &editor_buffer[current_line][current_col]);
        editor_buffer[current_line][current_col] = 0;
    }

    total_lines++;
    current_line++;
    current_col = 0;
    is_dirty = true;
    editor_redraw();
}

void editor_delete_char() {
    if(current_col > 0) {
        int len = strlen(editor_buffer[current_line]);
        for(int i = current_col - 1; i < len; i++) {
            editor_buffer[current_line][i] = editor_buffer[current_line][i+1];
        }
        current_col--;
        is_dirty = true;
        editor_redraw();
    } else if(current_line > 0) {
        int prev_len = strlen(editor_buffer[current_line - 1]);
        int curr_len = strlen(editor_buffer[current_line]);
        if(prev_len + curr_len < EDITOR_LINE_WIDTH) {
            strcat(editor_buffer[current_line - 1], editor_buffer[current_line]);
            for(int i = current_line; i < total_lines - 1; i++) {
                strcpy(editor_buffer[i], editor_buffer[i+1]);
            }
            total_lines--;
            current_line--;
            current_col = prev_len;
            is_dirty = true;
            editor_redraw();
        }
    }
}

void cmd_nano(char* filename) {
    if(strlen(filename) == 0) {
        println("Usage: nano <filename>");
        return;
    }

    strcpy(current_filename, filename);
    current_line = 0;
    current_col = 0;
    total_lines = 1;
    is_dirty = false;
    last_cursor_x = 0;
    last_cursor_y = 0;

    for(int i = 0; i < EDITOR_MAX_LINES; i++) {
        editor_buffer[i][0] = 0;
    }

    editor_load_file(filename);
    editor_redraw();

    while(1) {
        char c = get_key();
        if(c == 0) continue;

        if(c == 27) {
            if(is_dirty) {
                editor_save_file();
            }
            break;
        }

        if(c == '\n') {
            editor_new_line();
            continue;
        }

        if(c == '\b') {
            editor_delete_char();
            continue;
        }

        if(c == CHAR_ARROW_UP) {
            if(current_line > 0) current_line--;
            int len = strlen(editor_buffer[current_line]);
            if(current_col > len) current_col = len;
            editor_redraw();
            continue;
        }

        if(c == CHAR_ARROW_DOWN) {
            if(current_line < total_lines - 1) current_line++;
            int len = strlen(editor_buffer[current_line]);
            if(current_col > len) current_col = len;
            editor_redraw();
            continue;
        }

        if(c == CHAR_ARROW_LEFT) {
            if(current_col > 0) {
                current_col--;
                editor_redraw();
            }
            continue;
        }

        if(c == CHAR_ARROW_RIGHT) {
            int len = strlen(editor_buffer[current_line]);
            if(current_col < len) {
                current_col++;
                editor_redraw();
            }
            continue;
        }

        if(c >= 32 && c <= 126) {
            editor_insert_char(c);
        }
    }

    // Очищаем курсор перед выходом
    clear_block_cursor(last_cursor_x, last_cursor_y);
    clear_screen();
    println("Exited nano.");
}