#include "shell.h"
#include "../lib/screen.h"
#include "../lib/string.h"
#include "../lib/utils.h"
#include "../drivers/rtc.h"
#include "../drivers/fat32.h"
#include "builtin.h"
#include "../lib/scrollback.h"
#include "../commands/cmd_registry.h"

// Нужные символы из disk.h — объявляем вручную чтобы не тянуть конфликтующий disk.h
extern uint8_t sector_buffer[512];
extern "C" void ata_read_sector(uint32_t lba);

void cmd_theme(char* name);

// ─── Глобальные переменные ────────────────────────────────────────────────────

char buffer[BUF_SIZE];
int buf_len = 0;
int cursor_offset = 0;
char current_path[128] = "/";
char history[HISTORY_SIZE][BUF_SIZE] = {0};
int history_count = 0;
int history_index = -1;
int last_cursor_x = 0;
int last_cursor_y = 0;

static char  last_executed_command[BUF_SIZE] = {0};
static char  time_string[16] = "--:--:--";
static bool  status_bar_dirty = true;
static int   prompt_start_row = 1;   // строка экрана, с которой начинается текущий промпт

// ─── Scrollback view ─────────────────────────────────────────────────────────
// view_bottom: индекс scrollback-строки, которая отображается в НИЖНЕЙ строке экрана.
// -1 = живой вид.
#define SCROLL_CONTENT_ROWS 23          // строки 1..23 (строка 0 = статус-бар)
#define SCROLL_PAGE         3              // строк за одно нажатие PgUp/PgDn

static int  scroll_view         = -1;   // -1 = live, >=0 = scrollback
static uint16_t live_save[SCROLL_CONTENT_ROWS][80]; // сохранённый живой экран

// ─── Tab completion ───────────────────────────────────────────────────────────

#define TAB_MAX_MATCHES 16
static char tab_matches[TAB_MAX_MATCHES][128];
static int  tab_match_count = 0;
static int  tab_match_index = -1;
static char tab_prefix[128] = {0};
static int  tab_prefix_start = 0;

// Команды и Левенштейн теперь в cmd_registry.cpp
#include "../commands/cmd_registry.h"

// ─── Status bar ──────────────────────────────────────────────────────────────

void shell_init_status_bar() {
    strcpy(last_executed_command, "");
    strcpy(time_string, "--:--:--");
    status_bar_dirty = true;
    shell_draw_status_bar();
}

void shell_draw_status_bar() {
    uint16_t bar_attr = (theme_bar_bg << 4) | theme_bar_fg;
    uint16_t bar_val  = bar_attr << 8;

    for (int i = 0; i < 80; i++) video_memory[i] = bar_val | ' ';

    int pos = 1;
    int path_len = strlen(current_path);
    if (path_len > 25) path_len = 25;
    for (int i = 0; i < path_len; i++) video_memory[pos + i] = bar_val | current_path[i];
    pos += path_len;

    const char* cmd_label = " | CMD:";
    int ll = strlen(cmd_label);
    for (int i = 0; i < ll; i++) video_memory[pos + i] = bar_val | cmd_label[i];
    pos += ll;

    int cmd_len = strlen(last_executed_command);
    if (cmd_len > 28) cmd_len = 28;
    for (int i = 0; i < cmd_len; i++) video_memory[pos + i] = bar_val | last_executed_command[i];

    int right_pos = 68;
    const char* time_label = "T:";
    for (int i = 0; time_label[i]; i++) video_memory[right_pos + i] = bar_val | time_label[i];
    right_pos += 2;
    int time_len = strlen(time_string);
    if (time_len > 8) time_len = 8;
    for (int i = 0; i < time_len; i++) video_memory[right_pos + i] = bar_val | time_string[i];

    status_bar_dirty = false;
}

// Специальный статус-бар для режима прокрутки
static void scroll_draw_status_bar(int view_bot) {
    uint16_t bar_attr = (theme_bar_bg << 4) | theme_bar_fg;
    uint16_t bar_val  = bar_attr << 8;
    for (int i = 0; i < 80; i++) video_memory[i] = bar_val | ' ';

    const char* msg = " [SCROLL] PgUp=back  PgDn=fwd  Any key=exit";
    for (int i = 0; msg[i] && i < 55; i++) video_memory[i] = bar_val | msg[i];

    // позиция справа: "LINE N/M"
    int total = scrollback_count();
    const char* lbl = "LINE:";
    int rp = 57;
    for (int i = 0; lbl[i]; i++) video_memory[rp + i] = bar_val | lbl[i];
    rp += 5;

    // печатаем view_bot+1 / total в ячейки без printf
    char nbuf[12];
    auto write_num = [&](int n) {
        int start = 0;
        if (n <= 0) { nbuf[start++] = '0'; }
        else {
            int tmp = n, digits = 0;
            while (tmp > 0) { tmp /= 10; digits++; }
            for (int d = digits - 1; d >= 0; d--) { nbuf[d] = '0' + n % 10; n /= 10; }
            start = digits;
        }
        nbuf[start] = 0;
        for (int i = 0; nbuf[i] && rp + i < 80; i++) video_memory[rp + i] = bar_val | nbuf[i];
        rp += start;
    };
    write_num(view_bot + 1);
    if (rp < 80) video_memory[rp++] = bar_val | '/';
    write_num(total);
}

void shell_update_time() {
    rtc_get_time_string(time_string);
    if (scroll_view >= 0) return;   // в режиме скролла не обновляем

    uint16_t bar_attr = (theme_bar_bg << 4) | theme_bar_fg;
    uint16_t bar_val  = bar_attr << 8;
    int time_pos = 70;
    for (int i = 0; i < 8 && time_string[i]; i++)
        video_memory[time_pos + i] = bar_val | time_string[i];
}

void shell_save_executed_command(const char* cmd) {
    strcpy(last_executed_command, cmd);
    shell_draw_status_bar();
}

// ─── Scrollback view functions ────────────────────────────────────────────────

bool shell_is_in_scrollback() { return scroll_view >= 0; }

static void scroll_render() {
    int count = scrollback_count();
    uint8_t attr = get_theme_color();
    uint16_t blank = (attr << 8) | ' ';

    for (int r = 1; r <= SCROLL_CONTENT_ROWS; r++) {
        // верхняя строка экрана = самая старая из видимых
        // нижняя (r = SCROLL_CONTENT_ROWS) = scrollback[view_bottom]
        int offset = scroll_view + (SCROLL_CONTENT_ROWS - r);
        if (offset >= 0 && offset < count) {
            const uint16_t* line = scrollback_get(offset);
            for (int c = 0; c < 80; c++) video_memory[r * 80 + c] = line[c];
        } else {
            for (int c = 0; c < 80; c++) video_memory[r * 80 + c] = blank;
        }
    }

    scroll_draw_status_bar(scroll_view);
    // скрываем аппаратный курсор в режиме скролла
    update_vga_cursor(0, 25);
}

void shell_handle_pgup() {
    int count = scrollback_count();
    if (count == 0) return;

    if (scroll_view < 0) {
        // первый PgUp: сохраняем живой экран
        for (int r = 0; r < SCROLL_CONTENT_ROWS; r++)
            for (int c = 0; c < 80; c++)
                live_save[r][c] = video_memory[(r + 1) * 80 + c];
        scroll_view = 0;
    }

    int new_view = scroll_view + SCROLL_PAGE;
    // не уходить дальше чем есть строк (с запасом на верхние строки экрана)
    int max_view = count - 1;
    if (new_view > max_view) new_view = max_view;
    scroll_view = new_view;

    scroll_render();
}

void shell_handle_pgdn() {
    if (scroll_view < 0) return;   // уже в живом виде

    int new_view = scroll_view - SCROLL_PAGE;
    if (new_view < 0) {
        // возвращаемся к живому экрану
        scroll_view = -1;
        for (int r = 0; r < SCROLL_CONTENT_ROWS; r++)
            for (int c = 0; c < 80; c++)
                video_memory[(r + 1) * 80 + c] = live_save[r][c];
        shell_draw_status_bar();
        update_vga_cursor(last_cursor_x, last_cursor_y);
        return;
    }
    scroll_view = new_view;
    scroll_render();
}

// Вызывается при любой обычной клавише — выходим из режима скролла
void shell_exit_scrollback() {
    if (scroll_view < 0) return;
    scroll_view = -1;
    for (int r = 0; r < SCROLL_CONTENT_ROWS; r++)
        for (int c = 0; c < 80; c++)
            video_memory[(r + 1) * 80 + c] = live_save[r][c];
    shell_draw_status_bar();
    update_vga_cursor(last_cursor_x, last_cursor_y);
}

// ─── Shell core ───────────────────────────────────────────────────────────────

void shell_clear_line() {
    // Вычисляем сколько рядов занимает промпт + буфер
    int prompt_len  = strlen(current_path) + 2;
    int total_chars = prompt_len + buf_len;
    int rows_needed = total_chars / 80 + 1;  // +1 — ряд с курсором

    uint8_t attr = get_theme_color();
    uint16_t blank = (attr << 8) | ' ';

    for (int r = 0; r <= rows_needed; r++) {  // +1 запас
        int row = prompt_start_row + r;
        if (row < 1)  continue;
        if (row >= 25) break;
        for (int i = 0; i < 80; i++) video_memory[row * 80 + i] = blank;
    }

    cursor_pos = prompt_start_row * 80;
    update_vga_cursor(0, prompt_start_row);
}

void shell_print_prompt() {
    prompt_start_row = cursor_pos / 80;   // запоминаем ряд начала промпта
    if (prompt_start_row < 1) prompt_start_row = 1;
    print(current_path);
    print("$ ");
}

void shell_init_cursor() {
    int prompt_len = strlen(current_path) + 2;
    last_cursor_x = prompt_len;
    last_cursor_y = prompt_start_row;
    update_vga_cursor(last_cursor_x, last_cursor_y);
}

void tab_reset() {
    tab_match_count = 0;
    tab_match_index = -1;
    tab_prefix[0]   = 0;
}

void shell_redraw() {
    shell_draw_status_bar();
    shell_clear_line();
    shell_print_prompt();

    buffer[buf_len] = 0;
    print(buffer);

    int prompt_len = strlen(current_path) + 2;

    // Позиция курсора с учётом wrapping
    int abs_offset = prompt_len + cursor_offset;
    int cursor_row = prompt_start_row + abs_offset / 80;
    int cursor_col = abs_offset % 80;
    if (cursor_row >= 25) { cursor_row = 24; cursor_col = 79; }

    // Ghost-подсказка tab completion (тоже с учётом wrapping)
    if (tab_match_index >= 0 && tab_match_index < tab_match_count) {
        const char* full  = tab_matches[tab_match_index];
        const char* ghost = full + strlen(tab_prefix);
        int ghost_abs = prompt_len + buf_len;
        int ghost_row = prompt_start_row + ghost_abs / 80;
        int ghost_col = ghost_abs % 80;
        uint8_t gray_attr = 0x08;
        uint16_t gray_val = (gray_attr << 8);
        if (ghost_row < 25) {
            for (int i = 0; ghost[i] && (ghost_col + i) < 80; i++)
                video_memory[ghost_row * 80 + ghost_col + i] = gray_val | (uint8_t)ghost[i];
        }
    }

    last_cursor_x = cursor_col;
    last_cursor_y = cursor_row;
    update_vga_cursor(cursor_col, cursor_row);
    cursor_pos = cursor_row * 80 + cursor_col;
}

void shell_insert_char(char c) {
    if (buf_len >= BUF_SIZE - 1) return;
    tab_reset();
    for (int i = buf_len; i > cursor_offset; i--) buffer[i] = buffer[i-1];
    buffer[cursor_offset] = c;
    buf_len++;
    buffer[buf_len] = 0;
    cursor_offset++;
    shell_redraw();
}

void shell_delete_char() {
    if (cursor_offset == 0) return;
    tab_reset();
    for (int i = cursor_offset - 1; i < buf_len; i++) buffer[i] = buffer[i+1];
    buf_len--;
    cursor_offset--;
    buffer[buf_len] = 0;
    shell_redraw();
}

void shell_add_to_history(const char* cmd) {
    if (strlen(cmd) == 0) return;
    if (history_count > 0 && strcmp(history[HISTORY_SIZE - 1], cmd) == 0) return;
    for (int i = 0; i < HISTORY_SIZE - 1; i++) strcpy(history[i], history[i + 1]);
    strcpy(history[HISTORY_SIZE - 1], cmd);
    if (history_count < HISTORY_SIZE) history_count++;
}

void shell_load_history(int index) {
    if (index < 0) {
        for (int i = 0; i < BUF_SIZE; i++) buffer[i] = 0;
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

// ─── История: сохранение и загрузка с диска ───────────────────────────────────

static const char HIST_FILENAME[] = "HIST.DAT";

void shell_save_history_file() {
    static char hist_content[HISTORY_SIZE * BUF_SIZE + 2];
    int pos = 0;
    int start_idx = HISTORY_SIZE - history_count;
    for (int i = start_idx; i < HISTORY_SIZE; i++) {
        if (history[i][0] == 0) continue;
        int len = strlen(history[i]);
        for (int j = 0; j < len; j++) hist_content[pos++] = history[i][j];
        hist_content[pos++] = '\n';
    }
    hist_content[pos] = 0;

    // Всегда сохраняем в корне, независимо от текущей директории
    uint32_t saved_cluster = current_dir_cluster;
    current_dir_cluster = FAT32_ROOT_CLUSTER;

    if (fat32_find_entry(HIST_FILENAME, 0x00).found)
        fat32_delete_entry(HIST_FILENAME, 0x00);
    fat32_create_file(HIST_FILENAME, hist_content, pos);

    current_dir_cluster = saved_cluster;
}

void shell_load_history_file() {
    //println("Shell: Loading history file...");
    // Всегда читаем из корня
    uint32_t saved_cluster = current_dir_cluster;
    current_dir_cluster = FAT32_ROOT_CLUSTER;

    FAT32_FindResult res = fat32_find_entry(HIST_FILENAME, 0x00);
    if (!res.found) { current_dir_cluster = saved_cluster; return; }
    uint32_t size    = res.entry.file_size;
    uint32_t cluster = FAT32_GET_CLUSTER(&res.entry);
    if (size == 0 || cluster < 2) { current_dir_cluster = saved_cluster; return; }

    static char content[HISTORY_SIZE * BUF_SIZE + 4];
    uint32_t bytes_read = 0;
    uint32_t max_bytes  = (uint32_t)(HISTORY_SIZE * BUF_SIZE);

    while ((cluster & FAT32_MASK) < (FAT32_EOC & FAT32_MASK) &&
           bytes_read < size && bytes_read < max_bytes) {
        uint32_t lba = fat32_cluster_to_lba(cluster);
        for (int sec = 0; sec < FAT32_SECTORS_PER_CLUSTER &&
                          bytes_read < size && bytes_read < max_bytes; sec++) {
            ata_read_sector(lba + sec);
            for (int i = 0; i < 512 && bytes_read < size && bytes_read < max_bytes; i++)
                content[bytes_read++] = sector_buffer[i];
        }
        cluster = fat32_get_next_cluster(cluster);
    }
    content[bytes_read] = 0;

    // Парсим строки и добавляем в историю
    char line[BUF_SIZE];
    int  li = 0;
    for (uint32_t i = 0; i <= bytes_read; i++) {
        char ch = content[i];
        if (ch == '\n' || ch == '\r' || ch == 0) {
            if (li > 0) { line[li] = 0; shell_add_to_history(line); li = 0; }
        } else if (li < BUF_SIZE - 1) {
            line[li++] = ch;
        }
    }

    current_dir_cluster = saved_cluster;
    //println("Shell: History file loaded.");
}

static void get_last_word(char* out, int* start_out) {
    int start = 0;
    for (int i = 0; i < cursor_offset; i++)
        if (buffer[i] == ' ') start = i + 1;
    int i = 0;
    while (start + i < cursor_offset) { out[i] = buffer[start + i]; i++; }
    out[i] = 0;
    *start_out = start;
}

void shell_handle_tab(bool reverse) {
    // Цикл по уже найденным совпадениям
    if (tab_match_count > 0) {
        if (!reverse) tab_match_index = (tab_match_index + 1) % tab_match_count;
        else          tab_match_index = (tab_match_index - 1 + tab_match_count) % tab_match_count;
        shell_redraw();
        return;
    }

    char prefix[128] = {0};
    int  pstart = 0;
    get_last_word(prefix, &pstart);

    // Определяем: мы на первом слове (команда) или на аргументе (файл)?
    bool is_command_word = (pstart == 0);

    if (is_command_word) {
        // Дополнение по таблице команд
        tab_match_count = 0;
        int plen = strlen(prefix);
        for (int i = 0; i < CMD_TABLE_SIZE && tab_match_count < TAB_MAX_MATCHES; i++) {
            if (strncmp(CMD_TABLE[i].name, prefix, plen) == 0) {
                strcpy(tab_matches[tab_match_count], CMD_TABLE[i].name);
                tab_match_count++;
            }
        }
    } else {
        // Файловое дополнение только для ARG_FILE / ARG_TEXT команд с путём
        char cmd[32] = {0};
        int ci = 0;
        while (buffer[ci] && buffer[ci] != ' ' && ci < 31) { cmd[ci] = buffer[ci]; ci++; }
        cmd[ci] = 0;

        bool allow_file = false;
        for (int i = 0; i < CMD_TABLE_SIZE; i++) {
            if (strcmp(cmd, CMD_TABLE[i].name) == 0) {
                allow_file = (CMD_TABLE[i].arg_type == ARG_FILE);
                break;
            }
        }
        if (!allow_file) return;

        tab_match_count = fat32_tab_complete(prefix, tab_matches, TAB_MAX_MATCHES);
    }

    if (tab_match_count == 0) return;
    int k = 0; while (prefix[k]) { tab_prefix[k] = prefix[k]; k++; } tab_prefix[k] = 0;
    tab_prefix_start = pstart;
    tab_match_index  = reverse ? tab_match_count - 1 : 0;
    shell_redraw();
}

void shell_tab_accept() {
    if (tab_match_index < 0 || tab_match_index >= tab_match_count) return;
    const char* full   = tab_matches[tab_match_index];
    const char* suffix = full + strlen(tab_prefix);
    for (int i = 0; suffix[i]; i++) {
        if (buf_len < BUF_SIZE - 1) {
            for (int j = buf_len; j > cursor_offset; j--) buffer[j] = buffer[j-1];
            buffer[cursor_offset++] = suffix[i];
            buf_len++;
        }
    }
    buffer[buf_len] = 0;
    tab_reset();
    shell_redraw();
}

// Принять один символ из подсказки (Shift+Right)
void shell_tab_accept_one() {
    if (tab_match_index < 0 || tab_match_index >= tab_match_count) return;
    const char* full = tab_matches[tab_match_index];
    int prefix_len = strlen(tab_prefix);
    if (!full[prefix_len]) {          // подсказка закончилась — сбрасываем
        tab_reset();
        shell_redraw();
        return;
    }
    char c = full[prefix_len];
    if (buf_len < BUF_SIZE - 1) {
        for (int j = buf_len; j > cursor_offset; j--) buffer[j] = buffer[j-1];
        buffer[cursor_offset++] = c;
        buf_len++;
        buffer[buf_len] = 0;
    }
    // расширяем prefix на принятый символ
    tab_prefix[prefix_len] = c;
    tab_prefix[prefix_len + 1] = 0;
    // если prefix совпал с полным совпадением — сброс
    if (strcmp(tab_prefix, full) == 0) tab_reset();
    shell_redraw();
}

bool shell_has_suggestion() { return tab_match_index >= 0; }

// Проверяет, существует ли команда в таблице
static bool cmd_exists(const char* name) {
    int name_len = 0;
    while (name[name_len] && name[name_len] != ' ') name_len++;
    
    for (int i = 0; i < CMD_TABLE_SIZE; i++) {
        if (strncmp(CMD_TABLE[i].name, name, name_len) == 0 && 
            CMD_TABLE[i].name[name_len] == 0) {
            return true;
        }
    }
    return false;
}

void shell_redraw_full() {
    // Очищаем весь экран цветом темы
    uint8_t attr = get_theme_color();
    uint16_t blank = (attr << 8) | ' ';
    for (int i = 0; i < 80 * 25; i++) video_memory[i] = blank;

    // Перерисовываем статус-бар
    shell_draw_status_bar();

    // Сбрасываем cursor_pos на строку 1 (строка 0 = статус-бар).
    // Промпт и курсор нарисует вызывающий (main loop: shell_print_prompt + shell_init_cursor).
    cursor_pos = 80;
}

void shell_clear_buffer() {
    for (int i = 0; i < BUF_SIZE; i++) buffer[i] = 0;
    buf_len = 0;
    cursor_offset = 0;
}

// ─── Команды ─────────────────────────────────────────────────────────────────

void process_command() {
    buffer[buf_len] = 0;

    // ── !N — выполнить команду №N из истории ──────────────────────────────
    if (buffer[0] == '!' && buf_len > 1) {
        int n = 0;
        for (int i = 1; i < buf_len; i++)
            if (buffer[i] >= '0' && buffer[i] <= '9')
                n = n * 10 + (buffer[i] - '0');
        if (n >= 1 && n <= history_count) {
            int arr_idx = HISTORY_SIZE - history_count + (n - 1);
            strcpy(buffer, history[arr_idx]);
            buf_len = strlen(buffer);
        } else {
            print_char('\n');
            print("history: no event #");
            // печатаем n
            char nbuf[8]; int ni = 0;
            int tmp = n;
            if (tmp == 0) { nbuf[ni++] = '0'; }
            else {
                char rev[8]; int ri = 0;
                while (tmp > 0) { rev[ri++] = (char)('0' + tmp % 10); tmp /= 10; }
                for (int k = ri - 1; k >= 0; k--) nbuf[ni++] = rev[k];
            }
            nbuf[ni] = 0;
            println(nbuf);
            for (int i = 0; i < BUF_SIZE; i++) buffer[i] = 0;
            buf_len = 0; cursor_offset = 0;
            return;
        }
    }

    // ── Проверяем редирект вывода > и >> ───────────────────────────────────
    char redirect_file[64] = {0};
    char dispatch_buf[BUF_SIZE];
    bool has_redirect = false;
    bool append_mode  = false;
    int  dlen = strlen(buffer);

    // calc использует ">>" и ">" как побитовые операторы сдвига/сравнения —
    // для него редирект-парсинг всегда пропускаем, иначе "calc 8 >> 2" уходит
    // не на вычисление, а в попытку записи в файл "2".
    bool is_calc_cmd = (buffer[0]=='c' && buffer[1]=='a' && buffer[2]=='l' &&
                        buffer[3]=='c' && (buffer[4]==' ' || buffer[4]==0));

    if (is_calc_cmd) {
        for (int i = 0; i <= dlen; i++) dispatch_buf[i] = buffer[i];
    } else {
    for (int i = 0; i < dlen; i++) {
        if (buffer[i] == '>' && buffer[i+1] == '>') {
            // Копируем часть до ">>" как команду
            for (int j = 0; j < i; j++) dispatch_buf[j] = buffer[j];
            int clen = i;
            while (clen > 0 && dispatch_buf[clen - 1] == ' ') clen--;
            dispatch_buf[clen] = 0;
            // Имя файла — после ">>"
            const char* fn = buffer + i + 2;
            while (*fn == ' ') fn++;
            int fi = 0;
            while (fn[fi] && fn[fi] != ' ' && fi < 63)
                { redirect_file[fi] = fn[fi]; fi++; }
            redirect_file[fi] = 0;
            has_redirect = (fi > 0 && clen > 0);
            append_mode  = true;
            break;
        }

        if (buffer[i] == '>') {
            // Копируем часть до '>' как команду
            for (int j = 0; j < i; j++) dispatch_buf[j] = buffer[j];
            int clen = i;
            while (clen > 0 && dispatch_buf[clen - 1] == ' ') clen--;
            dispatch_buf[clen] = 0;
            // Имя файла — после '>'
            const char* fn = buffer + i + 1;
            while (*fn == ' ') fn++;
            int fi = 0;
            while (fn[fi] && fn[fi] != ' ' && fi < 63)
                { redirect_file[fi] = fn[fi]; fi++; }
            redirect_file[fi] = 0;
            has_redirect = (fi > 0 && clen > 0);
            break;
        }
    }
    }
    if (!has_redirect) {
        for (int i = 0; i <= dlen; i++) dispatch_buf[i] = buffer[i];
    }

    // Проверяем, что команда не пустая
    int cmd_len = 0;
    for (int i = 0; dispatch_buf[i]; i++) {
        if (dispatch_buf[i] != ' ') cmd_len++;
    }
    if (cmd_len == 0) {
        for (int i = 0; i < BUF_SIZE; i++) buffer[i] = 0;
        buf_len = 0;
        cursor_offset = 0;
        return;
    }

    // Добавляем в историю только если команда существует
    if (cmd_exists(dispatch_buf)) {
        shell_add_to_history(buffer);
        shell_save_history_file();    // сохраняем историю на диск
    }
    shell_save_executed_command(buffer);
    print_char('\n');

    if (has_redirect) {
        redirect_start();
        if (append_mode) {
            // Дозапись: сначала прогоняем существующее содержимое файла через
            // print_char (редирект уже активен -> он уйдёт в redirect_buf),
            // ДО того, как команда допишет свой вывод следом.
            FAT32_FindResult r = fat32_find_entry(redirect_file, 0x00);
            if (r.found && !(r.entry.attributes & 0x10)) {
                uint32_t size    = r.entry.file_size;
                uint32_t cluster = FAT32_GET_CLUSTER(&r.entry);
                uint32_t br = 0;
                while ((cluster & FAT32_MASK) >= 2 &&
                       (cluster & FAT32_MASK) < (FAT32_EOC & FAT32_MASK) && br < size) {
                    uint32_t lba = fat32_cluster_to_lba(cluster);
                    for (int s = 0; s < FAT32_SECTORS_PER_CLUSTER && br < size; s++) {
                        ata_read_sector(lba + s);
                        for (int b = 0; b < 512 && br < size; b++) { print_char(sector_buffer[b]); br++; }
                    }
                    cluster = fat32_get_next_cluster(cluster);
                }
            }
        }
    }
    cmd_dispatch(dispatch_buf);
    if (has_redirect) {
        int   rlen = redirect_get_len();
        const char* rout = redirect_get_buf();
        if (fat32_find_entry(redirect_file, 0x00).found)
            fat32_delete_entry(redirect_file, 0x00);
        bool ok = fat32_create_file(redirect_file, rout, rlen);
        redirect_stop();
        if (ok) { print("Saved "); print(redirect_file); print(" ("); 
                  // печатаем размер
                  char sb[8]; int si=0, sv=rlen;
                  if(sv==0){sb[si++]='0';}else{char r[8];int ri=0;
                    while(sv>0){r[ri++]=(char)('0'+sv%10);sv/=10;}
                    for(int k=ri-1;k>=0;k--)sb[si++]=r[k];}
                  sb[si]=0; print(sb); println(" bytes)"); }
        else println("Redirect: write failed.");
    }

    for (int i = 0; i < BUF_SIZE; i++) buffer[i] = 0;
    buf_len = 0;
    cursor_offset = 0;
}