#include "cmd_nano.h"
#include "../../kernel/screen.h"
#include "../../kernel/keyboard.h"
#include "../../fs/fat32.h"
#include "../../drivers/disk.h"
#include "../../lib/string.h"

#define EDITOR_MAX_LINES   200
#define EDITOR_LINE_WIDTH  79
#define EDITOR_VISIBLE_ROWS 22   // строки 0-21 — контент, 22-24 — статус/хелп
#undef  STATUS_BAR_ROW
#define STATUS_BAR_ROW     22
#define HELP_BAR_ROW       23

// Буфер
static char editor_buffer[EDITOR_MAX_LINES][EDITOR_LINE_WIDTH + 1];
static int  current_line  = 0;
static int  current_col   = 0;
static int  total_lines   = 1;
static int  scroll_offset = 0;   // первая видимая строка
static char current_filename[64] = {0};
static bool is_dirty = false;

static int last_cursor_x = 0;
static int last_cursor_y = 0;   // экранная Y (current_line - scroll_offset)

extern uint8_t sector_buffer[512];
extern void draw_block_cursor(int x, int y);
extern void clear_block_cursor(int x, int y);

// ─── helpers ──────────────────────────────────────────────────────────────────

static inline uint8_t text_attr() {
    return get_theme_color();
}

static inline uint8_t bar_attr() {
    return (uint8_t)((theme_bar_bg << 4) | (theme_bar_fg & 0x0F));
}

static void print_char_at(char c, int x, int y, uint8_t color) {
    video_memory[y * 80 + x] = (uint16_t)((color << 8) | (uint8_t)c);
}

static void print_at(const char* str, int x, int y, uint8_t color) {
    int pos = y * 80 + x;
    for(int i = 0; str[i]; i++)
        video_memory[pos++] = (uint16_t)((color << 8) | (uint8_t)str[i]);
}

static void editor_clear_content() {
    uint8_t attr = text_attr();
    for(int row = 0; row < EDITOR_VISIBLE_ROWS; row++)
        for(int col = 0; col < 80; col++)
            video_memory[row * 80 + col] = (uint16_t)((attr << 8) | ' ');
}

static void editor_print_hex(uint32_t n) {
    const char* hex = "0123456789ABCDEF";
    char buf[11];
    buf[0] = '0'; buf[1] = 'x';
    for(int i = 0; i < 8; i++) { buf[9-i] = hex[n & 0xF]; n >>= 4; }
    buf[10] = 0;
    print(buf);
}

// ─── прокрутка ────────────────────────────────────────────────────────────────

static void scroll_to_cursor() {
    if(current_line < scroll_offset)
        scroll_offset = current_line;
    else if(current_line >= scroll_offset + EDITOR_VISIBLE_ROWS)
        scroll_offset = current_line - EDITOR_VISIBLE_ROWS + 1;
}

// ─── статус-бар ───────────────────────────────────────────────────────────────

static void editor_draw_status_bar() {
    uint8_t bar = bar_attr();
    uint8_t txt = text_attr();

    // строка с именем файла
    for(int i = 0; i < 80; i++) print_char_at(' ', i, STATUS_BAR_ROW, bar);

    char status[80];
    int len = 0;
    const char* title = " NANO - ";
    for(int i = 0; title[i]; i++) status[len++] = title[i];
    for(int i = 0; current_filename[i] && len < 60; i++) status[len++] = current_filename[i];
    if(is_dirty) { status[len++] = ' '; status[len++] = '*'; }

    // позиция курсора справа
    char pos_str[20];
    int pl = 0;
    pos_str[pl++] = 'L'; pos_str[pl++] = ':';
    int ln = current_line + 1;
    if(ln >= 100) pos_str[pl++] = (char)('0' + ln/100);
    if(ln >= 10)  pos_str[pl++] = (char)('0' + (ln/10)%10);
    pos_str[pl++] = (char)('0' + ln%10);
    pos_str[pl++] = ' '; pos_str[pl++] = 'C'; pos_str[pl++] = ':';
    int cc = current_col + 1;
    if(cc >= 10) pos_str[pl++] = (char)('0' + cc/10);
    pos_str[pl++] = (char)('0' + cc%10);
    pos_str[pl++] = ' ';
    pos_str[pl] = 0;

    status[len] = 0;
    for(int i = 0; i < len; i++) print_char_at(status[i], i, STATUS_BAR_ROW, bar);
    int pstart = 80 - pl;
    for(int i = 0; i < pl; i++) print_char_at(pos_str[i], pstart + i, STATUS_BAR_ROW, bar);

    // строка подсказки
    for(int i = 0; i < 80; i++) print_char_at(' ', i, HELP_BAR_ROW, txt);
    const char* help = " ESC Save+Exit   Arrows Navigate   Lines: ";
    int hl = 0;
    for(; help[hl]; hl++) print_char_at(help[hl], hl, HELP_BAR_ROW, txt);

    // показываем общее кол-во строк
    char tl_str[8];
    int tl = 0;
    int t = total_lines;
    if(t >= 100) tl_str[tl++] = (char)('0' + t/100);
    if(t >= 10)  tl_str[tl++] = (char)('0' + (t/10)%10);
    tl_str[tl++] = (char)('0' + t%10);
    tl_str[tl] = 0;
    for(int i = 0; i < tl; i++) print_char_at(tl_str[i], hl + i, HELP_BAR_ROW, txt);

    // прокрутка: индикатор [scroll/max] справа
    if(total_lines > EDITOR_VISIBLE_ROWS) {
        const char* scr = " [SCROLL] ";
        int sl = strlen(scr);
        for(int i = 0; i < sl; i++)
            print_char_at(scr[i], 80 - sl + i, HELP_BAR_ROW, bar_attr());
    }
}

// ─── перерисовка ─────────────────────────────────────────────────────────────

static void editor_redraw() {
    clear_block_cursor(last_cursor_x, last_cursor_y);

    scroll_to_cursor();
    editor_clear_content();

    uint8_t attr = text_attr();
    for(int row = 0; row < EDITOR_VISIBLE_ROWS; row++) {
        int abs_line = scroll_offset + row;
        if(abs_line >= total_lines) break;
        if(editor_buffer[abs_line][0])
            print_at(editor_buffer[abs_line], 0, row, attr);
    }

    editor_draw_status_bar();

    int screen_y = current_line - scroll_offset;
    last_cursor_x = current_col;
    last_cursor_y = screen_y;

    draw_block_cursor(current_col, screen_y);
    update_vga_cursor(current_col, screen_y);
}

// ─── загрузка/сохранение ─────────────────────────────────────────────────────

static void editor_load_file(const char* filename) {
    println("Loading...");
    FAT32_FindResult result = fat32_find_entry(filename, 0x00);
    if(!result.found) { println("New file"); for(int i=0;i<1000000;i++); return; }

    uint32_t size    = result.entry.file_size;
    uint32_t cluster = FAT32_GET_CLUSTER(&result.entry);
    if(size == 0) { println("Empty file"); for(int i=0;i<1000000;i++); return; }

    static char file_content[EDITOR_MAX_LINES * (EDITOR_LINE_WIDTH + 1)];
    uint32_t bytes_read = 0;
    uint32_t max_bytes  = (uint32_t)(EDITOR_MAX_LINES * (EDITOR_LINE_WIDTH + 1) - 1);

    while((cluster & FAT32_MASK) < (FAT32_EOC & FAT32_MASK) && bytes_read < size) {
        uint32_t lba = fat32_cluster_to_lba(cluster);
        for(int sec = 0; sec < FAT32_SECTORS_PER_CLUSTER && bytes_read < size; sec++) {
            ata_read_sector(lba + sec);
            for(int i = 0; i < 512 && bytes_read < size && bytes_read < max_bytes; i++)
                file_content[bytes_read++] = sector_buffer[i];
        }
        cluster = fat32_get_next_cluster(cluster);
    }
    file_content[bytes_read] = 0;

    int line = 0, col = 0;
    for(uint32_t i = 0; i < bytes_read && line < EDITOR_MAX_LINES; i++) {
        char c = file_content[i];
        if(c == '\n')      { editor_buffer[line][col] = 0; line++; col = 0; }
        else if(c == '\r') { continue; }
        else if(col < EDITOR_LINE_WIDTH) { editor_buffer[line][col++] = c; }
    }
    if(col > 0 || line == 0) { editor_buffer[line][col] = 0; line++; }
    total_lines = line;

    print("Loaded "); editor_print_hex(bytes_read);
    print(" bytes, "); editor_print_hex(total_lines); println(" lines");
    for(int i=0;i<2000000;i++);
}

static void editor_save_file() {
    if(strlen(current_filename) == 0) {
        for(int i=0;i<80;i++) print_char_at(' ',i,21,0x0C);
        print_at("ERROR: No filename!",0,21,0x0C);
        return;
    }

    static char content[EDITOR_MAX_LINES * EDITOR_LINE_WIDTH];
    int pos = 0;
    for(int i = 0; i < total_lines; i++) {
        int len = strlen(editor_buffer[i]);
        for(int j = 0; j < len; j++) content[pos++] = editor_buffer[i][j];
        if(i < total_lines - 1) content[pos++] = '\n';
    }
    content[pos] = 0;

    editor_clear_content();
    for(int i=0;i<80;i++) print_char_at(' ',i,0,bar_attr());
    print_at(" Saving...",0,0,bar_attr());

    FAT32_FindResult existing = fat32_find_entry(current_filename, 0x00);
    if(existing.found) fat32_delete_entry(current_filename, 0x00);

    bool ok = fat32_create_file(current_filename, content, pos);
    if(ok) {
        print_at(" Saved OK ",0,0,bar_attr());
        is_dirty = false;
    } else {
        print_at(" SAVE FAILED! ",0,0,0x0C);
    }

    for(volatile int i=0;i<5000000;i++);
    editor_redraw();
}

// ─── редактирование ───────────────────────────────────────────────────────────

static void editor_insert_char(char c) {
    if(current_col >= EDITOR_LINE_WIDTH) return;
    int len = strlen(editor_buffer[current_line]);
    for(int i = len; i > current_col; i--)
        editor_buffer[current_line][i] = editor_buffer[current_line][i-1];
    editor_buffer[current_line][current_col] = c;
    editor_buffer[current_line][len+1] = 0;
    current_col++;
    is_dirty = true;
    editor_redraw();
}

static void editor_new_line() {
    if(total_lines >= EDITOR_MAX_LINES) return;
    for(int i = total_lines; i > current_line + 1; i--)
        strcpy(editor_buffer[i], editor_buffer[i-1]);

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

static void editor_delete_char() {
    if(current_col > 0) {
        int len = strlen(editor_buffer[current_line]);
        for(int i = current_col - 1; i < len; i++)
            editor_buffer[current_line][i] = editor_buffer[current_line][i+1];
        current_col--;
        is_dirty = true;
        editor_redraw();
    } else if(current_line > 0) {
        int prev_len = strlen(editor_buffer[current_line - 1]);
        int curr_len = strlen(editor_buffer[current_line]);
        if(prev_len + curr_len < EDITOR_LINE_WIDTH) {
            strcat(editor_buffer[current_line - 1], editor_buffer[current_line]);
            for(int i = current_line; i < total_lines - 1; i++)
                strcpy(editor_buffer[i], editor_buffer[i+1]);
            total_lines--;
            current_line--;
            current_col = prev_len;
            is_dirty = true;
            editor_redraw();
        }
    }
}

// ─── точка входа ─────────────────────────────────────────────────────────────

void cmd_nano(char* filename) {
    if(strlen(filename) == 0) { println("Usage: nano <filename>"); return; }

    strcpy(current_filename, filename);
    current_line  = 0;
    current_col   = 0;
    total_lines   = 1;
    scroll_offset = 0;
    is_dirty      = false;
    last_cursor_x = 0;
    last_cursor_y = 0;

    for(int i = 0; i < EDITOR_MAX_LINES; i++) editor_buffer[i][0] = 0;

    editor_load_file(filename);
    editor_redraw();

    while(1) {
        char c = get_key();
        if(c == 0) continue;

        if(c == 27) {
            if(is_dirty) editor_save_file();
            break;
        }

        if(c == '\n') { editor_new_line(); continue; }
        if(c == '\b') { editor_delete_char(); continue; }

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
            if(current_col > 0) { current_col--; editor_redraw(); }
            continue;
        }
        if(c == CHAR_ARROW_RIGHT) {
            int len = strlen(editor_buffer[current_line]);
            if(current_col < len) { current_col++; editor_redraw(); }
            continue;
        }

        if(c >= 32 && c <= 126) editor_insert_char(c);
    }

    clear_block_cursor(last_cursor_x, last_cursor_y);

    // полная очистка экрана с темой
    uint8_t attr = text_attr();
    for(int i = 0; i < 80 * 25; i++)
        video_memory[i] = (uint16_t)((attr << 8) | ' ');
    cursor_pos = 80;

    shell_init_status_bar();
    println("Exited nano.");
}