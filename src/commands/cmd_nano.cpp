#include "cmd_nano.h"
#include "../lib/screen.h"
#include "../kernel/keyboard.h"
#include "../drivers/fat32.h"
#include "../drivers/disk.h"
#include "../lib/string.h"

#define EDITOR_MAX_LINES    200
#define EDITOR_LINE_WIDTH   79
#define EDITOR_VISIBLE_ROWS 22
#undef  STATUS_BAR_ROW
#define STATUS_BAR_ROW      22
#define HELP_BAR_ROW        23
#define LINE_NUM_WIDTH       4

// ─── undo/redo ────────────────────────────────────────────────────────────────
#define UNDO_MAX 64
struct UndoEntry {
    char lines[EDITOR_MAX_LINES][EDITOR_LINE_WIDTH + 1];
    int  total_lines, cur_line, cur_col;
};
static UndoEntry undo_stack[UNDO_MAX];
static int undo_top = -1, redo_top = -1;

// ─── буфер ───────────────────────────────────────────────────────────────────
static char editor_buffer[EDITOR_MAX_LINES][EDITOR_LINE_WIDTH + 1];
static int  current_line = 0, current_col = 0;
static int  total_lines  = 1, scroll_offset = 0;
static char current_filename[64] = {0};
static bool is_dirty = false;
static int  last_cursor_x = 0, last_cursor_y = 0;

// ─── выделение ───────────────────────────────────────────────────────────────
static bool sel_active   = false;
static int  sel_line_start = 0, sel_col_start = 0;
static int  sel_line_end   = 0, sel_col_end   = 0;

// Буфер копирования: до 32 строк по EDITOR_LINE_WIDTH символов + '\n'
#define CLIPBOARD_SIZE (32 * (EDITOR_LINE_WIDTH + 2))
static char clipboard[CLIPBOARD_SIZE] = {0};
static int  clipboard_len = 0;

// ─── поиск ───────────────────────────────────────────────────────────────────
static char search_query[64] = {0};
static bool search_active    = false;
static int  search_results[EDITOR_MAX_LINES];
static int  search_result_count = 0, search_result_idx = 0, search_query_len = 0;
#define SEARCH_HIGHLIGHT_ATTR  0x0E   // жёлтый на чёрном
// SEL_ATTR вычисляется динамически через sel_get_attr()

extern uint8_t sector_buffer[512];
extern void draw_block_cursor(int x, int y);
extern void clear_block_cursor(int x, int y);

// ─── helpers ─────────────────────────────────────────────────────────────────
static inline uint8_t text_attr() { return get_theme_color(); }
static inline uint8_t bar_attr()  { return (uint8_t)((theme_bar_bg << 4) | (theme_bar_fg & 0x0F)); }
// Цвет выделения = инверсия текущей темы (fg↔bg) — заметно на любой теме
static inline uint8_t sel_attr()  {
    uint8_t fg = theme_fg & 0x0F;
    uint8_t bg = theme_bg & 0x0F;
    // Если fg == bg (нечитаемо) — добавляем яркость к тексту
    if (fg == bg) fg = (fg ^ 0x0F) & 0x0F;
    return (uint8_t)((fg << 4) | bg);
}

static void print_char_at(char c, int x, int y, uint8_t color) {
    video_memory[y * 80 + x] = (uint16_t)((color << 8) | (uint8_t)c);
}
static void print_at(const char* str, int x, int y, uint8_t color) {
    int pos = y * 80 + x;
    for(int i = 0; str[i]; i++) video_memory[pos++] = (uint16_t)((color << 8) | (uint8_t)str[i]);
}
static void editor_clear_content() {
    uint8_t a = text_attr();
    for(int r = 0; r < EDITOR_VISIBLE_ROWS; r++)
        for(int c = 0; c < 80; c++)
            video_memory[r * 80 + c] = (uint16_t)((a << 8) | ' ');
}
static void editor_print_hex(uint32_t n) {
    const char* h = "0123456789ABCDEF";
    char buf[11]; buf[0]='0'; buf[1]='x';
    for(int i=0;i<8;i++){buf[9-i]=h[n&0xF];n>>=4;} buf[10]=0; print(buf);
}

// ─── выделение: нормализация (start <= end) ──────────────────────────────────
static void sel_normalize(int& sl, int& sc, int& el, int& ec) {
    sl = sel_line_start; sc = sel_col_start;
    el = sel_line_end;   ec = sel_col_end;
    // Если курсор "до" начала — переставляем
    if (sl > el || (sl == el && sc > ec)) {
        int tl=sl,tc=sc; sl=el; sc=ec; el=tl; ec=tc;
    }
}

// Проверяет, попадает ли символ (line, col) в выделение
static bool in_selection(int line, int col) {
    if (!sel_active) return false;
    int sl,sc,el,ec; sel_normalize(sl,sc,el,ec);
    if (line < sl || line > el) return false;
    if (sl == el) return col >= sc && col < ec;  // однострочное
    if (line == sl) return col >= sc;             // первая строка: от sc до конца
    if (line == el) return col < ec;              // последняя строка: от начала до ec
    return true;                                  // средние строки: целиком
}

// ─── undo/redo ────────────────────────────────────────────────────────────────
static void undo_save() {
    if (undo_top >= UNDO_MAX - 1) {
        for(int i=0;i<UNDO_MAX-1;i++) undo_stack[i]=undo_stack[i+1];
        undo_top=UNDO_MAX-2;
    }
    undo_top++; redo_top=undo_top;
    UndoEntry* e=&undo_stack[undo_top];
    for(int i=0;i<total_lines;i++) strcpy(e->lines[i],editor_buffer[i]);
    e->total_lines=total_lines; e->cur_line=current_line; e->cur_col=current_col;
}
static void undo_restore(const UndoEntry* e) {
    for(int i=0;i<e->total_lines;i++) strcpy(editor_buffer[i],e->lines[i]);
    total_lines=e->total_lines; current_line=e->cur_line; current_col=e->cur_col;
    is_dirty=true; sel_active=false;
}
static void do_undo() { if(undo_top>0){ undo_top--; undo_restore(&undo_stack[undo_top]); } }
static void do_redo() { if(undo_top<redo_top){ undo_top++; undo_restore(&undo_stack[undo_top]); } }

// ─── прокрутка ───────────────────────────────────────────────────────────────
static void scroll_to_cursor() {
    if(current_line < scroll_offset) scroll_offset=current_line;
    else if(current_line >= scroll_offset+EDITOR_VISIBLE_ROWS)
        scroll_offset=current_line-EDITOR_VISIBLE_ROWS+1;
}

// ─── статус-бар ──────────────────────────────────────────────────────────────
static void editor_draw_status_bar() {
    uint8_t bar=bar_attr(), txt=text_attr();
    for(int i=0;i<80;i++) print_char_at(' ',i,STATUS_BAR_ROW,bar);

    char status[80]; int len=0;
    const char* title=" NANO - ";
    for(int i=0;title[i];i++) status[len++]=title[i];
    for(int i=0;current_filename[i]&&len<55;i++) status[len++]=current_filename[i];
    if(is_dirty){status[len++]=' ';status[len++]='*';}
    if(sel_active){
        const char* s=" [SEL]";
        for(int i=0;s[i]&&len<70;i++) status[len++]=s[i];
    }

    char pos_str[20]; int pl=0;
    pos_str[pl++]='L'; pos_str[pl++]=':';
    int ln=current_line+1;
    if(ln>=100) pos_str[pl++]=(char)('0'+ln/100);
    if(ln>=10)  pos_str[pl++]=(char)('0'+(ln/10)%10);
    pos_str[pl++]=(char)('0'+ln%10);
    pos_str[pl++]=' '; pos_str[pl++]='C'; pos_str[pl++]=':';
    int cc=current_col+1;
    if(cc>=10) pos_str[pl++]=(char)('0'+cc/10);
    pos_str[pl++]=(char)('0'+cc%10);
    pos_str[pl++]=' '; pos_str[pl]=0;

    status[len]=0;
    for(int i=0;i<len;i++) print_char_at(status[i],i,STATUS_BAR_ROW,bar);
    int ps=80-pl;
    for(int i=0;i<pl;i++) print_char_at(pos_str[i],ps+i,STATUS_BAR_ROW,bar);

    for(int i=0;i<80;i++) print_char_at(' ',i,HELP_BAR_ROW,txt);
    const char* help=" ESC Save?  S-ESC Quit  ^ESC Save  ^F Search  ^Z/^Y Undo/Redo  ^C Copy  ^V Paste  Lines: ";
    int hl=0;
    for(;help[hl];hl++) print_char_at(help[hl],hl,HELP_BAR_ROW,txt);

    char tl_str[8]; int tl=0; int t=total_lines;
    if(t>=100) tl_str[tl++]=(char)('0'+t/100);
    if(t>=10)  tl_str[tl++]=(char)('0'+(t/10)%10);
    tl_str[tl++]=(char)('0'+t%10); tl_str[tl]=0;
    for(int i=0;i<tl;i++) print_char_at(tl_str[i],hl+i,HELP_BAR_ROW,txt);

    if(total_lines>EDITOR_VISIBLE_ROWS){
        const char* scr=" [SCROLL] "; int sl2=strlen(scr);
        for(int i=0;i<sl2;i++) print_char_at(scr[i],80-sl2+i,HELP_BAR_ROW,bar_attr());
    }
}

// ─── перерисовка ─────────────────────────────────────────────────────────────
static void editor_redraw() {
    clear_block_cursor(last_cursor_x,last_cursor_y);
    scroll_to_cursor();
    editor_clear_content();

    uint8_t attr=text_attr(), num_attr=bar_attr();
    uint8_t s_attr=sel_attr();

    for(int row=0;row<EDITOR_VISIBLE_ROWS;row++) {
        int abs_line=scroll_offset+row;
        if(abs_line>=total_lines) break;

        int n=abs_line+1;
        char nb[5];
        nb[0]=(n>=100)?(char)('0'+n/100):' ';
        nb[1]=(n>=10) ?(char)('0'+(n/10)%10):' ';
        nb[2]=(char)('0'+n%10); nb[3]=' ';
        for(int i=0;i<LINE_NUM_WIDTH;i++) print_char_at(nb[i],i,row,num_attr);

        int line_len=strlen(editor_buffer[abs_line]);
        int max_disp=80-LINE_NUM_WIDTH;
        if(line_len>max_disp) line_len=max_disp;

        int ci=0;
        while(ci<line_len) {
            // Приоритет: выделение > поиск > обычный
            uint8_t a=attr;
            if(in_selection(abs_line,ci)) {
                a=s_attr;
            } else if(search_active && search_query_len>0 && ci+search_query_len<=line_len) {
                bool match=true;
                for(int qi=0;qi<search_query_len&&match;qi++)
                    if(editor_buffer[abs_line][ci+qi]!=search_query[qi]) match=false;
                if(match) a=SEARCH_HIGHLIGHT_ATTR;
            }

            // Если текущий символ часть найденного совпадения — рисуем весь блок
            if(a==SEARCH_HIGHLIGHT_ATTR) {
                for(int qi=0;qi<search_query_len&&ci+qi<line_len;qi++)
                    print_char_at(editor_buffer[abs_line][ci+qi],
                                  ci+qi+LINE_NUM_WIDTH,row,a);
                ci+=search_query_len;
            } else {
                print_char_at(editor_buffer[abs_line][ci],ci+LINE_NUM_WIDTH,row,a);
                ci++;
            }
        }
    }

    editor_draw_status_bar();
    int sy=current_line-scroll_offset;
    last_cursor_x=current_col+LINE_NUM_WIDTH; last_cursor_y=sy;
    draw_block_cursor(current_col+LINE_NUM_WIDTH,sy);
    update_vga_cursor(current_col+LINE_NUM_WIDTH,sy);
}

// ─── загрузка ────────────────────────────────────────────────────────────────
static void editor_load_file(const char* filename) {
    println("Loading...");
    FAT32_FindResult result=fat32_find_entry(filename,0x00);
    if(!result.found){println("New file");for(int i=0;i<1000000;i++);return;}
    uint32_t size=result.entry.file_size;
    uint32_t cluster=FAT32_GET_CLUSTER(&result.entry);
    if(size==0){println("Empty file");for(int i=0;i<1000000;i++);return;}

    static char file_content[EDITOR_MAX_LINES*(EDITOR_LINE_WIDTH+1)];
    uint32_t bytes_read=0, max_bytes=(uint32_t)(EDITOR_MAX_LINES*(EDITOR_LINE_WIDTH+1)-1);

    while((cluster&FAT32_MASK)<(FAT32_EOC&FAT32_MASK)&&bytes_read<size){
        uint32_t lba=fat32_cluster_to_lba(cluster);
        for(int sec=0;sec<FAT32_SECTORS_PER_CLUSTER&&bytes_read<size;sec++){
            ata_read_sector(lba+sec);
            for(int i=0;i<512&&bytes_read<size&&bytes_read<max_bytes;i++)
                file_content[bytes_read++]=sector_buffer[i];
        }
        cluster=fat32_get_next_cluster(cluster);
    }
    file_content[bytes_read]=0;

    int line=0,col=0;
    for(uint32_t i=0;i<bytes_read&&line<EDITOR_MAX_LINES;i++){
        char c=file_content[i];
        if(c=='\n'){editor_buffer[line][col]=0;line++;col=0;}
        else if(c=='\r'){continue;}
        else if(col<EDITOR_LINE_WIDTH){editor_buffer[line][col++]=c;}
    }
    if(col>0||line==0){editor_buffer[line][col]=0;line++;}
    total_lines=line;

    print("Loaded ");editor_print_hex(bytes_read);
    print(" bytes, ");editor_print_hex(total_lines);println(" lines");
    for(int i=0;i<2000000;i++);
}

// ─── сохранение ──────────────────────────────────────────────────────────────
static void editor_save_file() {
    if(strlen(current_filename)==0){
        for(int i=0;i<80;i++) print_char_at(' ',i,21,0x0C);
        print_at("ERROR: No filename!",0,21,0x0C); return;
    }
    static char content[EDITOR_MAX_LINES*EDITOR_LINE_WIDTH];
    int pos=0;
    for(int i=0;i<total_lines;i++){
        int len=strlen(editor_buffer[i]);
        for(int j=0;j<len;j++) content[pos++]=editor_buffer[i][j];
        if(i<total_lines-1) content[pos++]='\n';
    }
    content[pos]=0;

    editor_clear_content();
    for(int i=0;i<80;i++) print_char_at(' ',i,0,bar_attr());
    print_at(" Saving...",0,0,bar_attr());

    FAT32_FindResult existing=fat32_find_entry(current_filename,0x00);
    if(existing.found) fat32_delete_entry(current_filename,0x00);
    bool ok=fat32_create_file(current_filename,content,pos);
    if(ok){print_at(" Saved OK ",0,0,bar_attr());is_dirty=false;}
    else  {print_at(" SAVE FAILED! ",0,0,0x0C);}
    for(volatile int i=0;i<5000000;i++);
    editor_redraw();
}

// ─── диалог выхода (ESC) ─────────────────────────────────────────────────────
// Возвращает true если надо выходить, false — остаться
static bool editor_exit_dialog() {
    if(!is_dirty) return true;   // нечего спрашивать

    uint8_t bar=bar_attr();
    for(int i=0;i<80;i++) print_char_at(' ',i,HELP_BAR_ROW,bar);
    const char* msg=" Save before exit? [Y]es / [N]o / [C]ancel";
    for(int i=0;msg[i];i++) print_char_at(msg[i],i,HELP_BAR_ROW,bar);

    while(1){
        uint8_t k=0; while(k==0) k=get_key();
        if(k=='y'||k=='Y'){ editor_save_file(); return true; }
        if(k=='n'||k=='N') return true;   // выйти без сохранения
        if(k=='c'||k=='C'||k==27) { editor_draw_status_bar(); return false; }
    }
}

// ─── редактирование ──────────────────────────────────────────────────────────
static void editor_insert_char(char c) {
    if(current_col>=EDITOR_LINE_WIDTH) return;
    undo_save();
    sel_active=false;
    int len=strlen(editor_buffer[current_line]);
    for(int i=len;i>current_col;i--)
        editor_buffer[current_line][i]=editor_buffer[current_line][i-1];
    editor_buffer[current_line][current_col]=c;
    editor_buffer[current_line][len+1]=0;
    current_col++; is_dirty=true; editor_redraw();
}
static void editor_new_line() {
    if(total_lines>=EDITOR_MAX_LINES) return;
    undo_save(); sel_active=false;
    for(int i=total_lines;i>current_line+1;i--)
        strcpy(editor_buffer[i],editor_buffer[i-1]);
    editor_buffer[current_line+1][0]=0;
    if(current_col<(int)strlen(editor_buffer[current_line])){
        strcpy(editor_buffer[current_line+1],&editor_buffer[current_line][current_col]);
        editor_buffer[current_line][current_col]=0;
    }
    total_lines++; current_line++; current_col=0;
    is_dirty=true; editor_redraw();
}
static void editor_delete_char() {
    if(current_col>0){
        undo_save(); sel_active=false;
        int len=strlen(editor_buffer[current_line]);
        for(int i=current_col-1;i<len;i++)
            editor_buffer[current_line][i]=editor_buffer[current_line][i+1];
        current_col--; is_dirty=true; editor_redraw();
    } else if(current_line>0){
        int pl=strlen(editor_buffer[current_line-1]);
        int cl=strlen(editor_buffer[current_line]);
        if(pl+cl<EDITOR_LINE_WIDTH){
            undo_save(); sel_active=false;
            strcat(editor_buffer[current_line-1],editor_buffer[current_line]);
            for(int i=current_line;i<total_lines-1;i++)
                strcpy(editor_buffer[i],editor_buffer[i+1]);
            total_lines--; current_line--; current_col=pl;
            is_dirty=true; editor_redraw();
        }
    }
}
static void editor_delete_forward() {
    int len=strlen(editor_buffer[current_line]);
    if(current_col<len){
        undo_save(); sel_active=false;
        for(int i=current_col;i<len;i++)
            editor_buffer[current_line][i]=editor_buffer[current_line][i+1];
        is_dirty=true; editor_redraw();
    } else if(current_line<total_lines-1){
        int nl=strlen(editor_buffer[current_line+1]);
        if(len+nl<EDITOR_LINE_WIDTH){
            undo_save(); sel_active=false;
            strcat(editor_buffer[current_line],editor_buffer[current_line+1]);
            for(int i=current_line+1;i<total_lines-1;i++)
                strcpy(editor_buffer[i],editor_buffer[i+1]);
            editor_buffer[total_lines-1][0]=0;
            total_lines--; is_dirty=true; editor_redraw();
        }
    }
}

// ─── выделение ───────────────────────────────────────────────────────────────
// Начало выделения: запоминаем точку старта
static void sel_start() {
    if(!sel_active){
        sel_active=true;
        sel_line_start=current_line; sel_col_start=current_col;
    }
    // Обновляем конец (= текущая позиция курсора ПОСЛЕ перемещения)
}
static void sel_update_end() {
    sel_line_end=current_line; sel_col_end=current_col;
}

// Ctrl+C — копировать выделенный текст в clipboard
static void editor_copy() {
    if(!sel_active) return;  // нет выделения — clipboard не трогаем
    int sl,sc,el,ec; sel_normalize(sl,sc,el,ec);

    clipboard_len=0;
    for(int ln=sl; ln<=el && clipboard_len<CLIPBOARD_SIZE-2; ln++){
        int from = (ln==sl) ? sc : 0;
        int to   = (ln==el) ? ec : (int)strlen(editor_buffer[ln]);
        for(int ci=from; ci<to && clipboard_len<CLIPBOARD_SIZE-2; ci++)
            clipboard[clipboard_len++]=editor_buffer[ln][ci];
        if(ln<el && clipboard_len<CLIPBOARD_SIZE-2)
            clipboard[clipboard_len++]='\n';
    }
    clipboard[clipboard_len]=0;
    sel_active=false;
    editor_redraw();
}

// Ctrl+V — вставить clipboard в текущую позицию
static void editor_paste() {
    if(clipboard_len==0) return;
    undo_save();
    for(int ci=0; ci<clipboard_len; ci++){
        char c=clipboard[ci];
        if(c=='\n'){
            editor_new_line();
        } else {
            // Вставляем символ без вызова undo_save внутри editor_insert_char
            if(current_col<EDITOR_LINE_WIDTH){
                int len=strlen(editor_buffer[current_line]);
                for(int i=len;i>current_col;i--)
                    editor_buffer[current_line][i]=editor_buffer[current_line][i-1];
                editor_buffer[current_line][current_col]=c;
                editor_buffer[current_line][len+1]=0;
                current_col++;
            }
        }
    }
    is_dirty=true;
    editor_redraw();
}

// ─── поиск ───────────────────────────────────────────────────────────────────
static void search_collect_results() {
    search_result_count=0;
    search_query_len=strlen(search_query);
    if(search_query_len==0) return;
    for(int ln=0;ln<total_lines;ln++)
        if(strstr(editor_buffer[ln],search_query))
            search_results[search_result_count++]=ln;
}
static void search_draw_bar() {
    uint8_t bar=bar_attr();
    for(int i=0;i<80;i++) print_char_at(' ',i,HELP_BAR_ROW,bar);
    int x=0;
    print_char_at('[',x++,HELP_BAR_ROW,bar);
    int cur=search_result_count>0?search_result_idx+1:0;
    if(cur>=10) print_char_at((char)('0'+cur/10),x++,HELP_BAR_ROW,bar);
    print_char_at((char)('0'+cur%10),x++,HELP_BAR_ROW,bar);
    print_char_at('/',x++,HELP_BAR_ROW,bar);
    int tot=search_result_count;
    if(tot>=10) print_char_at((char)('0'+tot/10),x++,HELP_BAR_ROW,bar);
    print_char_at((char)('0'+tot%10),x++,HELP_BAR_ROW,bar);
    print_char_at(']',x++,HELP_BAR_ROW,bar);
    const char* hint=" ESC Exit  Up/Dn Prev/Next : ";
    for(int i=0;hint[i]&&x<80;i++) print_char_at(hint[i],x++,HELP_BAR_ROW,bar);
    for(int i=0;search_query[i]&&x<80;i++)
        print_char_at(search_query[i],x++,HELP_BAR_ROW,(uint8_t)SEARCH_HIGHLIGHT_ATTR);
}
static void search_goto_current() {
    if(search_result_count==0) return;
    current_line=search_results[search_result_idx];
    char* pos=strstr(editor_buffer[current_line],search_query);
    current_col=pos?(int)(pos-editor_buffer[current_line]):0;
    scroll_to_cursor();
}
static void editor_search() {
    uint8_t bar=bar_attr();
    for(int i=0;i<80;i++) print_char_at(' ',i,HELP_BAR_ROW,bar);
    const char* prompt=" Search: ";
    int px=0;
    for(;prompt[px];px++) print_char_at(prompt[px],px,HELP_BAR_ROW,bar);

    char query[64]; int qlen=0; query[0]=0;
    while(1){
        for(int i=px;i<80;i++) print_char_at(' ',i,HELP_BAR_ROW,bar);
        for(int i=0;i<qlen;i++) print_char_at(query[i],px+i,HELP_BAR_ROW,bar);
        print_char_at('_',px+qlen,HELP_BAR_ROW,bar);
        uint8_t c=0; while(c==0) c=get_key();
        if(c==27){editor_draw_status_bar();return;}
        if(c=='\n') break;
        if(c=='\b'){if(qlen>0){qlen--;query[qlen]=0;}continue;}
        if(c>=32&&c<=126&&qlen<63){query[qlen++]=(char)c;query[qlen]=0;}
    }
    if(qlen==0){editor_draw_status_bar();return;}
    strcpy(search_query,query);

    search_collect_results();
    if(search_result_count==0){
        for(int i=0;i<80;i++) print_char_at(' ',i,HELP_BAR_ROW,bar);
        const char* msg=" Not found. Press any key...";
        for(int i=0;msg[i];i++) print_char_at(msg[i],i,HELP_BAR_ROW,bar);
        uint8_t k=0; while(k==0) k=get_key();
        editor_draw_status_bar(); return;
    }
    search_result_idx=0;
    for(int i=0;i<search_result_count;i++)
        if(search_results[i]>=current_line){search_result_idx=i;break;}
    search_active=true;
    search_goto_current();
    editor_redraw();
    search_draw_bar();

    while(1){
        uint8_t c=0; while(c==0) c=get_key();
        if(c==27){search_active=false;search_query_len=0;editor_redraw();return;}
        if(c==CHAR_ARROW_DOWN||c==CHAR_ARROW_RIGHT){
            if(search_result_count>0){
                search_result_idx=(search_result_idx+1)%search_result_count;
                search_goto_current();editor_redraw();search_draw_bar();
            }
            continue;
        }
        if(c==CHAR_ARROW_UP||c==CHAR_ARROW_LEFT){
            if(search_result_count>0){
                search_result_idx=(search_result_idx-1+search_result_count)%search_result_count;
                search_goto_current();editor_redraw();search_draw_bar();
            }
            continue;
        }
        search_active=false;search_query_len=0;editor_redraw();return;
    }
}

// ─── точка входа ─────────────────────────────────────────────────────────────
void cmd_nano(char* filename) {
    if(strlen(filename)==0){println("Usage: nano <filename>");return;}

    strcpy(current_filename,filename);
    current_line=0;current_col=0;total_lines=1;scroll_offset=0;
    is_dirty=false;last_cursor_x=0;last_cursor_y=0;
    sel_active=false;clipboard_len=0;
    search_active=false;search_query_len=0;search_query[0]=0;
    undo_top=-1;redo_top=-1;

    for(int i=0;i<EDITOR_MAX_LINES;i++) editor_buffer[i][0]=0;
    editor_load_file(filename);

    undo_top=0;redo_top=0;
    UndoEntry* e0=&undo_stack[0];
    for(int i=0;i<total_lines;i++) strcpy(e0->lines[i],editor_buffer[i]);
    e0->total_lines=total_lines;e0->cur_line=0;e0->cur_col=0;

    editor_redraw();

    while(1){
        uint8_t c=get_key();
        if(c==0) continue;

        // ── Выход ────────────────────────────────────────────────────────────
        if(c==CHAR_SHIFT_ESC) break;                    // Shift+ESC — без сохранения
        if(c==CHAR_CTRL_ESC) { editor_save_file(); break; } // Ctrl+ESC — сохранить
        if(c==27) { if(editor_exit_dialog()) break; continue; } // ESC — спросить

        // ── Правка ───────────────────────────────────────────────────────────
        if(c=='\n')            { editor_new_line();       continue; }
        if(c=='\b')            { editor_delete_char();    continue; }
        if(c==CHAR_DEL)        { editor_delete_forward(); continue; }
        if(c==CHAR_CTRL_F)     { editor_search();         continue; }
        if(c==CHAR_CTRL_Z)     { do_undo(); editor_redraw(); continue; }
        if(c==CHAR_CTRL_SHIFT_Z){ do_redo(); editor_redraw(); continue; }
        if(c==CHAR_CTRL_C)     { editor_copy();           continue; }
        if(c==CHAR_CTRL_V)     { editor_paste();          continue; }

        // ── Навигация ─────────────────────────────────────────────────────────
        if(c==CHAR_ARROW_UP||c==CHAR_SHIFT_UP) {
            bool sh=(c==CHAR_SHIFT_UP);
            if(sh) sel_start();
            else   sel_active=false;
            if(current_line>0) current_line--;
            int len=strlen(editor_buffer[current_line]);
            if(current_col>len) current_col=len;
            if(sh) sel_update_end();
            editor_redraw(); continue;
        }
        if(c==CHAR_ARROW_DOWN||c==CHAR_SHIFT_DOWN) {
            bool sh=(c==CHAR_SHIFT_DOWN);
            if(sh) sel_start();
            else   sel_active=false;
            if(current_line<total_lines-1) current_line++;
            int len=strlen(editor_buffer[current_line]);
            if(current_col>len) current_col=len;
            if(sh) sel_update_end();
            editor_redraw(); continue;
        }
        if(c==CHAR_ARROW_LEFT||c==CHAR_SHIFT_LEFT) {
            bool sh=(c==CHAR_SHIFT_LEFT);
            if(sh) sel_start();
            else   sel_active=false;
            if(current_col>0) current_col--;
            if(sh) sel_update_end();
            editor_redraw(); continue;
        }
        if(c==CHAR_ARROW_RIGHT||c==CHAR_SHIFT_RIGHT) {
            bool sh=(c==CHAR_SHIFT_RIGHT);
            if(sh) sel_start();
            else   sel_active=false;
            int len=strlen(editor_buffer[current_line]);
            if(current_col<len) current_col++;
            if(sh) sel_update_end();
            editor_redraw(); continue;
        }

        // ── Обычный символ ───────────────────────────────────────────────────
        if(c>=32&&c<=126){ sel_active=false; editor_insert_char((char)c); }
    }

    clear_block_cursor(last_cursor_x,last_cursor_y);
    uint8_t attr=text_attr();
    for(int i=0;i<80*25;i++) video_memory[i]=(uint16_t)((attr<<8)|' ');
    cursor_pos=80;
    shell_init_status_bar();
    println("Exited nano.");
}