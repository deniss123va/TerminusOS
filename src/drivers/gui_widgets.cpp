#include "gui_widgets.h"
#include "vga_gfx.h"
#include "../kernel/mouse.h"
#include "../kernel/keyboard.h"

// ─── Состояние кадра ─────────────────────────────────────────────────────────
static bool    s_mouse_down_now  = false;
static bool    s_mouse_down_prev = false;
static bool    s_click_edge      = false;
static bool    s_release_edge    = false;
static uint8_t s_key             = 0;
// Позиция курсора для этого кадра (из gfx_cursor_get_x/y — всегда совпадает с визуальным)
static int s_cur_frame_x = 0, s_cur_frame_y = 0;

static bool s_dragging       = false;
static int  s_drag_widget_id = 0;

static bool s_menu_open = false;
static int  s_menu_x = 0, s_menu_y = 0, s_menu_w = 0, s_menu_h = 0;

void gui_begin_frame(uint8_t key) {
    s_key             = key;
    s_mouse_down_prev = s_mouse_down_now;
    s_mouse_down_now  = (mouse_state.buttons & 0x01) != 0;
    s_click_edge      = s_mouse_down_now && !s_mouse_down_prev;
    s_release_edge    = !s_mouse_down_now && s_mouse_down_prev;
    if (s_release_edge) { s_dragging = false; s_drag_widget_id = 0; }
    // Позиция = визуальный курсор, а не физическая мышь
    s_cur_frame_x = gfx_cursor_get_x();
    s_cur_frame_y = gfx_cursor_get_y();
}

bool gui_mouse_down()   { return s_mouse_down_now; }
bool gui_click_edge()   { return s_click_edge;     }
bool gui_release_edge() { return s_release_edge;   }

bool gui_point_in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x+w && py >= y && py < y+h;
}

// ─── КНОПКА ──────────────────────────────────────────────────────────────────
bool gui_button(int x, int y, int w, int h, const char* label) {
    bool hovered = gui_point_in_rect(s_cur_frame_x, s_cur_frame_y, x, y, w, h);
    bool pressed = hovered && s_mouse_down_now;

    uint8_t fill   = pressed ? GFX_DARK_GRAY : GFX_LIGHT_GRAY;
    uint8_t elo    = pressed ? GFX_WHITE     : GFX_DARK_GRAY;
    uint8_t ehi    = pressed ? GFX_DARK_GRAY : GFX_WHITE;

    gfx_fill_rect(x, y, w, h, fill);
    gfx_draw_hline(x,       y,       w, elo);
    gfx_draw_vline(x,       y,       h, elo);
    gfx_draw_hline(x,       y+h-1,   w, ehi);
    gfx_draw_vline(x+w-1,   y,       h, ehi);

    int len = 0; while (label[len]) len++;
    int tx = x + (w - len*8) / 2;
    int ty = y + (h - 8) / 2;
    if (tx < x+2) tx = x+2;
    int off = pressed ? 1 : 0;
    gfx_draw_text(tx+off, ty+off, label, GFX_BLACK, fill);

    return hovered && s_click_edge;
}

// ─── ЧЕКБОКС ─────────────────────────────────────────────────────────────────
bool gui_checkbox(int x, int y, const char* label, bool* checked) {
    const int BOX = 10;
    int len = 0; while (label[len]) len++;
    bool hovered = gui_point_in_rect(s_cur_frame_x, s_cur_frame_y,
                                      x, y, BOX + 4 + len*8, BOX);

    gfx_fill_rect(x, y, BOX, BOX, GFX_WHITE);
    gfx_draw_rect(x, y, BOX, BOX, GFX_BLACK);
    if (*checked) {
        gfx_draw_line(x+2, y+5, x+4, y+7, GFX_BLACK);
        gfx_draw_line(x+4, y+7, x+8, y+2, GFX_BLACK);
    }
    gfx_draw_text(x+BOX+4, y+1, label,
                  hovered ? GFX_YELLOW : GFX_WHITE, GFX_BLACK);

    if (hovered && s_click_edge) { *checked = !*checked; return true; }
    return false;
}

// ─── ТЕКСТОВОЕ ПОЛЕ ──────────────────────────────────────────────────────────
bool gui_textfield(int x, int y, int w, char* buf, int buf_size, bool* focused) {
    const int H = 12;
    bool hovered = gui_point_in_rect(s_cur_frame_x, s_cur_frame_y, x, y, w, H);
    if (s_click_edge) *focused = hovered;

    // Считаем длину до обработки клавиши
    int len = 0;
    while (buf[len]) len++;

    bool submitted = false;
    if (*focused && s_key) {
        if (s_key == '\b') {
            if (len > 0) buf[--len] = 0;          // len сразу обновился
        } else if (s_key == '\n' || s_key == '\r') {
            submitted = true;
        } else if (s_key >= 32 && s_key < 127) {
            if (len < buf_size - 1) {
                buf[len++] = (char)s_key;          // len сразу обновился
                buf[len]   = 0;
            }
        }
    }

    uint8_t border = *focused ? GFX_YELLOW : GFX_LIGHT_GRAY;
    gfx_fill_rect(x, y, w, H, GFX_BLACK);
    gfx_draw_rect(x, y, w, H, border);

    int max_chars = (w - 4) / 8;
    if (max_chars < 0) max_chars = 0;
    const char* show = buf;
    int show_len = len;
    if (show_len > max_chars) {
        show    += (show_len - max_chars);
        show_len = max_chars;
    }
    gfx_draw_text(x+2, y+2, show, GFX_WHITE, GFX_BLACK);

    // Курсор — позиция ПОСЛЕ обновлённого текста
    if (*focused) {
        int cx = x + 2 + show_len * 8;
        if (cx < x + w - 2)
            gfx_draw_vline(cx, y+2, 8, GFX_YELLOW);
    }
    return submitted;
}

// ─── СЛАЙДЕР ─────────────────────────────────────────────────────────────────
bool gui_slider(int x, int y, int w, float* value) {
    const int TRACK_H = 6, HANDLE_W = 12, HANDLE_H = 14;
    const int TOTAL_H = HANDLE_H + 2;
    int track_y = y + (TOTAL_H - TRACK_H) / 2;

    float v = *value;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;

    int handle_x = x + (int)(v * (w - HANDLE_W));
    bool hovered = gui_point_in_rect(s_cur_frame_x, s_cur_frame_y, x, y, w, TOTAL_H);
    int widget_id = x * 7919 + y * 104729;

    if (s_click_edge && hovered) { s_dragging = true; s_drag_widget_id = widget_id; }

    bool changed = false;
    if (s_dragging && s_drag_widget_id == widget_id) {
        int mx = s_cur_frame_x;
        if (mx < x) mx = x;
        if (mx > x + w - HANDLE_W) mx = x + w - HANDLE_W;
        float nv = (float)(mx - x) / (float)(w - HANDLE_W);
        if (nv < 0.0f) nv = 0.0f;
        if (nv > 1.0f) nv = 1.0f;
        if (nv != v) { *value = nv; v = nv; handle_x = x+(int)(nv*(w-HANDLE_W)); changed = true; }
    }

    gfx_fill_rect(x, track_y, w, TRACK_H, GFX_DARK_GRAY);
    gfx_draw_rect(x, track_y, w, TRACK_H, GFX_BLACK);
    int filled_w = handle_x - x + HANDLE_W/2;
    if (filled_w > 0 && filled_w < w)
        gfx_fill_rect(x+1, track_y+1, filled_w-2, TRACK_H-2, GFX_LIGHT_BLUE);

    bool hp = s_dragging && s_drag_widget_id == widget_id;
    uint8_t hfill = hp ? GFX_LIGHT_GRAY : GFX_WHITE;
    uint8_t elo   = hp ? GFX_DARK_GRAY  : GFX_WHITE;
    uint8_t ehi   = hp ? GFX_WHITE      : GFX_DARK_GRAY;
    gfx_fill_rect(handle_x, y, HANDLE_W, HANDLE_H, hfill);
    gfx_draw_hline(handle_x,        y,            HANDLE_W, elo);
    gfx_draw_vline(handle_x,        y,            HANDLE_H, elo);
    gfx_draw_hline(handle_x,        y+HANDLE_H-1, HANDLE_W, ehi);
    gfx_draw_vline(handle_x+HANDLE_W-1, y,        HANDLE_H, ehi);

    gfx_draw_text(x,           y+TOTAL_H+2, "0",   GFX_LIGHT_GRAY, GFX_BLACK);
    gfx_draw_text(x+w-3*8,     y+TOTAL_H+2, "100", GFX_LIGHT_GRAY, GFX_BLACK);
    return changed;
}

// ─── ТОГЛ ────────────────────────────────────────────────────────────────────
bool gui_toggle(int x, int y, const char* label, bool* checked) {
    const int W=28, H=14, PAD=2;
    const int HR = (H - PAD*2) / 2;
    int len=0; while(label[len]) len++;
    bool hovered = gui_point_in_rect(s_cur_frame_x, s_cur_frame_y,
                                      x, y, W+6+len*8, H);

    uint8_t bg = *checked ? GFX_LIGHT_GREEN : GFX_DARK_GRAY;
    gfx_fill_rect(x+2, y,   W-4, H,   bg);
    gfx_fill_rect(x,   y+2, W,   H-4, bg);
    gfx_put_pixel(x+1,   y+1,   bg); gfx_put_pixel(x+W-2, y+1,   bg);
    gfx_put_pixel(x+1,   y+H-2, bg); gfx_put_pixel(x+W-2, y+H-2, bg);

    gfx_draw_hline(x+2,   y,     W-4, GFX_BLACK);
    gfx_draw_hline(x+2,   y+H-1, W-4, GFX_BLACK);
    gfx_draw_vline(x,     y+2,   H-4, GFX_BLACK);
    gfx_draw_vline(x+W-1, y+2,   H-4, GFX_BLACK);
    gfx_put_pixel(x+1,   y,     GFX_BLACK); gfx_put_pixel(x+W-2, y,     GFX_BLACK);
    gfx_put_pixel(x+1,   y+H-1, GFX_BLACK); gfx_put_pixel(x+W-2, y+H-1, GFX_BLACK);

    int hcx = *checked ? (x+W-HR-PAD-1) : (x+HR+PAD+1);
    int hcy = y + H/2;
    int hx  = hcx-HR, hy = hcy-HR, hw = HR*2+1;
    gfx_fill_rect(hx+1, hy,   hw-2, hw,   GFX_WHITE);
    gfx_fill_rect(hx,   hy+1, hw,   hw-2, GFX_WHITE);
    gfx_put_pixel(hx+1,    hy+1,    GFX_WHITE); gfx_put_pixel(hx+hw-2, hy+1,    GFX_WHITE);
    gfx_put_pixel(hx+1,    hy+hw-2, GFX_WHITE); gfx_put_pixel(hx+hw-2, hy+hw-2, GFX_WHITE);
    gfx_draw_hline(hx+1,    hy,       hw-2, GFX_LIGHT_GRAY);
    gfx_draw_vline(hx,      hy+1,     hw-2, GFX_LIGHT_GRAY);
    gfx_draw_hline(hx+1,    hy+hw-1,  hw-2, GFX_DARK_GRAY);
    gfx_draw_vline(hx+hw-1, hy+1,     hw-2, GFX_DARK_GRAY);

    gfx_draw_text(x+W+6, y+(H-8)/2+1, label,
                  hovered ? GFX_YELLOW : GFX_WHITE, GFX_BLACK);

    if (hovered && s_click_edge) { *checked = !*checked; return true; }
    return false;
}

// ─── ПРОГРЕСС-БАР ────────────────────────────────────────────────────────────
void gui_progressbar(int x, int y, int w, float value, uint8_t color) {
    const int H = 12;
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    gfx_fill_rect(x, y, w, H, GFX_DARK_GRAY);
    gfx_draw_rect(x, y, w, H, GFX_BLACK);

    int filled = (int)(value * (float)(w - 2));
    if (filled > 0) gfx_fill_rect(x+1, y+1, filled, H-2, color);

    // Процент в центре
    int pct = (int)(value * 100.0f);
    char tmp[5];
    int ti = 0;
    if (pct >= 100) { tmp[ti++]='1'; tmp[ti++]='0'; tmp[ti++]='0'; }
    else if (pct >= 10) { tmp[ti++]=(char)('0'+pct/10); tmp[ti++]=(char)('0'+pct%10); }
    else               { tmp[ti++]=(char)('0'+pct); }
    tmp[ti++]='%'; tmp[ti]=0;
    int tx = x + (w - ti*8) / 2;
    if (tx < x+1) tx = x+1;
    // Цвет текста: белый если заполнено, тёмный если нет
    int split = x + 1 + filled; // x где заливка заканчивается
    if (tx + ti*8 < split)
        gfx_draw_text(tx, y+2, tmp, GFX_WHITE, color);       // весь текст на заливке
    else if (tx > split)
        gfx_draw_text(tx, y+2, tmp, GFX_LIGHT_GRAY, GFX_DARK_GRAY); // весь на фоне
    else {
        // Два прохода с разными цветами (приближённо — просто белый)
        gfx_draw_text(tx, y+2, tmp, GFX_WHITE, GFX_TRANSPARENT);
    }
}

// ─── КОНТЕКСТНОЕ МЕНЮ ────────────────────────────────────────────────────────
bool gui_context_menu(int x, int y, const char** items, int item_count, int* out_index) {
    const int ITEM_H=14, PAD_X=6, PAD_Y=2;
    int max_w = 60;
    for (int i=0; i<item_count; i++) {
        int len=0; while(items[i][len]) len++;
        int tw = len*8 + PAD_X*2;
        if (tw > max_w) max_w = tw;
    }
    int menu_w = max_w;
    int menu_h = item_count*ITEM_H + PAD_Y*2;

    if (!s_menu_open) {
        s_menu_open=true; s_menu_x=x; s_menu_y=y; s_menu_w=menu_w; s_menu_h=menu_h;
    }

    bool click_outside = s_click_edge && !gui_point_in_rect(
        s_cur_frame_x, s_cur_frame_y, s_menu_x, s_menu_y, s_menu_w, s_menu_h);
    if (click_outside) { s_menu_open=false; *out_index=-1; return false; }

    gfx_fill_rect(s_menu_x, s_menu_y, s_menu_w, s_menu_h, GFX_LIGHT_GRAY);
    gfx_draw_rect(s_menu_x, s_menu_y, s_menu_w, s_menu_h, GFX_WHITE);
    gfx_draw_rect(s_menu_x+1, s_menu_y+1, s_menu_w-2, s_menu_h-2, GFX_DARK_GRAY);

    bool selected=false; *out_index=-1;
    for (int i=0; i<item_count; i++) {
        int iy=s_menu_y+PAD_Y+i*ITEM_H, ix=s_menu_x+PAD_X, iw=s_menu_w-PAD_X*2;
        bool ih=gui_point_in_rect(s_cur_frame_x, s_cur_frame_y, ix, iy, iw, ITEM_H);
        if (ih) gfx_fill_rect(ix, iy, iw, ITEM_H, GFX_LIGHT_BLUE);
        gfx_draw_text(ix+2, iy+3, items[i], ih ? GFX_WHITE : GFX_BLACK, GFX_LIGHT_GRAY);
        if (ih && s_click_edge) { *out_index=i; selected=true; s_menu_open=false; }
    }
    gfx_draw_hline(s_menu_x, s_menu_y+s_menu_h, s_menu_w, GFX_DARK_GRAY);
    return selected;
}
void gui_context_menu_close()    { s_menu_open = false; }
bool gui_context_menu_is_open()  { return s_menu_open;  }

// ─── Вспомогательная: рисуем скроллбар ───────────────────────────────────────
static void draw_vscrollbar(int sb_x, int sb_y, int sb_h,
                             int visible, int total, int offset,
                             int scrollbar_w) {
    gfx_fill_rect(sb_x, sb_y, scrollbar_w, sb_h, GFX_DARK_GRAY);
    gfx_draw_rect(sb_x, sb_y, scrollbar_w, sb_h, GFX_BLACK);
    if (total > visible) {
        float ratio = (float)visible / (float)total;
        int th = (int)(ratio * sb_h); if (th < 8) th = 8;
        float sr = (float)offset / (float)(total - visible);
        int ty = sb_y + (int)(sr * (sb_h - th));
        if (ty < sb_y) ty = sb_y;
        if (ty+th > sb_y+sb_h) ty = sb_y+sb_h-th;
        gfx_fill_rect(sb_x+1, ty, scrollbar_w-2, th, GFX_LIGHT_GRAY);
        gfx_draw_rect(sb_x+1, ty, scrollbar_w-2, th, GFX_WHITE);
    } else {
        gfx_fill_rect(sb_x+1, sb_y, scrollbar_w-2, sb_h, GFX_LIGHT_GRAY);
    }
}

// ─── ТЕКСТОВАЯ СКРОЛЛ-ПАНЕЛЬ ─────────────────────────────────────────────────
bool gui_scroll_panel(int x, int y, int w, int panel_h,
                      const char** lines, int line_count,
                      int* scroll_y, int* scroll_x) {
    const int LINE_H = 10;
    const int SB_W   = 8;   // вертикальный скроллбар
    const int HSB_H  = 8;   // горизонтальный скроллбар

    int content_w   = w - SB_W - 2;
    int content_h_px= panel_h - HSB_H - 2;
    int visible_lines = content_h_px / LINE_H;
    if (visible_lines < 1) visible_lines = 1;
    int max_chars_vis = content_w / 8;  // сколько символов влезает по ширине

    // Максимальная длина строки — нужна ЗАРАНЕЕ, чтобы клампить scroll_x.
    // Раньше считалась только перед рисованием горизонтального скроллбара,
    // а сама прокрутка клампилась лишь с одной стороны (>0), поэтому стрелка
    // "вправо" крутила scroll_x до бесконечности независимо от длины текста.
    int max_line_len = 0;
    for (int i=0; i<line_count; i++) {
        int l=0; while(lines[i][l]) l++;
        if (l > max_line_len) max_line_len = l;
    }
    int max_scroll_x = max_line_len - max_chars_vis;
    if (max_scroll_x < 0) max_scroll_x = 0;

    bool mouse_in = gui_point_in_rect(s_cur_frame_x, s_cur_frame_y, x, y, w, panel_h);
    bool changed = false;

    // Вертикальный скролл (стрелки вверх/вниз)
    if (mouse_in) {
        if (s_key == CHAR_ARROW_UP   && *scroll_y > 0) { (*scroll_y)--; changed=true; }
        if (s_key == CHAR_ARROW_DOWN && *scroll_y < line_count-visible_lines)
            { (*scroll_y)++; changed=true; }
        // Горизонтальный скролл (стрелки влево/вправо)
        if (s_key == CHAR_ARROW_LEFT  && *scroll_x > 0) { (*scroll_x)--; changed=true; }
        if (s_key == CHAR_ARROW_RIGHT && *scroll_x < max_scroll_x) { (*scroll_x)++; changed=true; }
    }

    // Средняя кнопка мыши — вертикальный скролл
    static int s_wprev = 0;
    bool mid = (mouse_state.buttons & 0x04) != 0;
    if (mid && mouse_in) {
        int dy = s_cur_frame_y - s_wprev;
        if (dy >  3 && *scroll_y < line_count-visible_lines) { (*scroll_y)++; changed=true; s_wprev=s_cur_frame_y; }
        if (dy < -3 && *scroll_y > 0)                         { (*scroll_y)--; changed=true; s_wprev=s_cur_frame_y; }
    } else { s_wprev = s_cur_frame_y; }

    // Ограничения
    if (*scroll_y < 0) *scroll_y = 0;
    if (*scroll_y > line_count-visible_lines) {
        *scroll_y = line_count-visible_lines;
        if (*scroll_y < 0) *scroll_y = 0;
    }
    if (*scroll_x < 0) *scroll_x = 0;
    if (*scroll_x > max_scroll_x) *scroll_x = max_scroll_x;

    // --- Рисуем фон ---
    gfx_fill_rect(x, y, w, panel_h, GFX_BLACK);
    gfx_draw_rect(x, y, w, panel_h, GFX_LIGHT_GRAY);

    // --- Рисуем строки с клиппингом ---
    gfx_set_clip(x+1, y+1, content_w, content_h_px);
    for (int i = 0; i < visible_lines; i++) {
        int li = *scroll_y + i;
        if (li >= line_count) break;
        const char* line = lines[li];
        // Горизонтальный сдвиг: пропускаем scroll_x символов
        int skip = *scroll_x;
        while (skip > 0 && *line) { line++; skip--; }
        gfx_draw_text(x+2, y+2+i*LINE_H, line, GFX_WHITE, GFX_BLACK);
    }
    gfx_clear_clip();

    // --- Вертикальный скроллбар ---
    draw_vscrollbar(x+w-SB_W-1, y+1, panel_h-HSB_H-2,
                    visible_lines, line_count, *scroll_y, SB_W);

    // --- Горизонтальный скроллбар ---
    // max_line_len уже посчитан выше (нужен был раньше для клампа scroll_x)
    int hsb_x = x+1, hsb_y = y+panel_h-HSB_H-1, hsb_w = w-SB_W-2;
    gfx_fill_rect(hsb_x, hsb_y, hsb_w, HSB_H, GFX_DARK_GRAY);
    gfx_draw_rect(hsb_x, hsb_y, hsb_w, HSB_H, GFX_BLACK);
    if (max_line_len > max_chars_vis) {
        float r = (float)max_chars_vis / (float)max_line_len;
        int tw = (int)(r * hsb_w); if (tw < 8) tw = 8;
        float sr = (float)*scroll_x / (float)(max_line_len - max_chars_vis);
        int tx = hsb_x + (int)(sr * (hsb_w - tw));
        if (tx < hsb_x) tx = hsb_x;
        if (tx+tw > hsb_x+hsb_w) tx = hsb_x+hsb_w-tw;
        gfx_fill_rect(tx+1, hsb_y+1, tw-2, HSB_H-2, GFX_LIGHT_GRAY);
        gfx_draw_rect(tx+1, hsb_y+1, tw-2, HSB_H-2, GFX_WHITE);
    } else {
        gfx_fill_rect(hsb_x+1, hsb_y+1, hsb_w-2, HSB_H-2, GFX_LIGHT_GRAY);
    }

    return changed;
}

// ─── СКРОЛЛ-ПАНЕЛЬ С ПРОИЗВОЛЬНЫМ КОНТЕНТОМ ──────────────────────────────────
static int s_sb_scroll_prev = 0;

int gui_scroll_begin(int x, int y, int w, int h,
                     int content_total_h, int* scroll_y) {
    const int SB_W = 8;
    int content_w = w - SB_W - 1;

    bool mouse_in = gui_point_in_rect(s_cur_frame_x, s_cur_frame_y, x, y, w, h);

    // Прокрутка клавишами
    if (mouse_in) {
        if (s_key == CHAR_ARROW_UP   && *scroll_y > 0)
            (*scroll_y) -= 8;
        if (s_key == CHAR_ARROW_DOWN && *scroll_y < content_total_h - h)
            (*scroll_y) += 8;
    }

    // Средняя кнопка
    bool mid = (mouse_state.buttons & 0x04) != 0;
    if (mid && mouse_in) {
        int dy = s_cur_frame_y - s_sb_scroll_prev;
        if (dy >  3) { (*scroll_y) += 8; s_sb_scroll_prev = s_cur_frame_y; }
        if (dy < -3) { (*scroll_y) -= 8; s_sb_scroll_prev = s_cur_frame_y; }
    } else { s_sb_scroll_prev = s_cur_frame_y; }

    if (*scroll_y < 0) *scroll_y = 0;
    if (*scroll_y > content_total_h - h) {
        *scroll_y = content_total_h - h;
        if (*scroll_y < 0) *scroll_y = 0;
    }

    // Фон + рамка
    gfx_fill_rect(x, y, w, h, GFX_BLACK);
    gfx_draw_rect(x, y, w, h, GFX_LIGHT_GRAY);

    // Активируем клиппинг на область контента
    gfx_set_clip(x+1, y+1, content_w, h-2);

    // Возвращаем Y-начало контента (виджеты рисуют относительно него)
    return (y + 1) - *scroll_y;
}

void gui_scroll_end(int x, int y, int w, int h,
                    int content_total_h, int scroll_y) {
    const int SB_W = 8;
    gfx_clear_clip();
    // Вертикальный скроллбар
    draw_vscrollbar(x+w-SB_W-1, y+1, h-2,
                    h, content_total_h, scroll_y, SB_W);
}
