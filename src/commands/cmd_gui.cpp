#include "cmd_gui.h"
#include "../drivers/vga_gfx.h"
#include "../drivers/gui_widgets.h"
#include "../kernel/keyboard.h"
#include "../kernel/mouse.h"
#include "../lib/screen.h"
#include "../shell/shell.h"
#include "../drivers/serial.h"

// ─── Размеры ─────────────────────────────────────────────────────────────────
static const int TOOLBAR_H = 28;   // высота панели инструментов сверху
static const int STATUS_H  = 12;   // строка статуса снизу

// ─── Холст ───────────────────────────────────────────────────────────────────
static const int CANVAS_W = 640;
static uint8_t canvas[640 * 480];  // весь экран как холст (обрежем по факту)

static void canvas_clear(int canvas_h, uint8_t color) {
    for (int i = 0; i < CANVAS_W * canvas_h; i++) canvas[i] = color;
}

// Рисуем квадратным пером (pen_size нечётный: 1,3,5)
static void canvas_draw(int cx, int cy, int canvas_h,
                        int pen_size, uint8_t color) {
    int half = pen_size / 2;
    for (int dy = -half; dy <= half; dy++)
        for (int dx = -half; dx <= half; dx++) {
            int px = cx + dx, py = cy + dy;
            if (px >= 0 && px < CANVAS_W && py >= 0 && py < canvas_h)
                canvas[py * CANVAS_W + px] = color;
        }
}

// Bresenham между двумя точками (чтобы не было пробелов при быстром движении)
static void canvas_line(int x0, int y0, int x1, int y1,
                        int canvas_h, int pen_size, uint8_t color) {
    int dx = x1 - x0; if (dx < 0) dx = -dx;
    int dy = y1 - y0; if (dy < 0) dy = -dy;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (true) {
        canvas_draw(x0, y0, canvas_h, pen_size, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

// ─── Блит холста в backbuffer ─────────────────────────────────────────────────
static void blit_canvas(int W, int canvas_y, int canvas_h) {
    uint8_t* bb = gfx_get_backbuffer();
    for (int row = 0; row < canvas_h; row++) {
        const uint8_t* src = canvas + row * CANVAS_W;
        uint8_t*       dst = bb + (canvas_y + row) * W;
        for (int col = 0; col < W; col++) dst[col] = src[col];
    }
}

// ─── Toolbar ──────────────────────────────────────────────────────────────────
// Палитра 16 цветов
static const uint8_t PALETTE[16] = {
    GFX_BLACK,      GFX_BLUE,       GFX_GREEN,      GFX_CYAN,
    GFX_RED,        GFX_MAGENTA,    GFX_BROWN,      GFX_LIGHT_GRAY,
    GFX_DARK_GRAY,  GFX_LIGHT_BLUE, GFX_LIGHT_GREEN,GFX_LIGHT_CYAN,
    GFX_LIGHT_RED,  GFX_LIGHT_MAGENTA, GFX_YELLOW,  GFX_WHITE,
};

static void draw_toolbar(int W, uint8_t cur_color, int pen_size,
                         bool* clicked_clear, uint8_t* new_color,
                         int* new_pen) {
    // Фон toolbar
    gfx_fill_rect(0, 0, W, TOOLBAR_H, GFX_DARK_GRAY);
    gfx_draw_hline(0, TOOLBAR_H - 1, W, GFX_BLACK);

    // Заголовок
    gfx_draw_text(4, 10, "Paint", GFX_WHITE, GFX_DARK_GRAY);

    // Текущий цвет (большой квадрат)
    const int CUR_X = 52, CUR_Y = 3, CUR_SZ = 22;
    gfx_fill_rect(CUR_X, CUR_Y, CUR_SZ, CUR_SZ, cur_color);
    gfx_draw_rect(CUR_X, CUR_Y, CUR_SZ, CUR_SZ, GFX_WHITE);

    // 16 цветов
    const int PAL_X = 82, PAL_Y = 4, SW = 16, SH = 9, GAP = 1;
    for (int i = 0; i < 16; i++) {
        int col = i % 8, row = i / 8;
        int bx = PAL_X + col * (SW + GAP);
        int by = PAL_Y + row * (SH + GAP);
        gfx_fill_rect(bx, by, SW, SH, PALETTE[i]);
        if (PALETTE[i] == cur_color)
            gfx_draw_rect(bx - 1, by - 1, SW + 2, SH + 2, GFX_WHITE);
        // Клик на цвет
        if (gui_click_edge() &&
            gui_point_in_rect(gfx_cursor_get_x(), gfx_cursor_get_y(),
                              bx, by, SW, SH))
            *new_color = PALETTE[i];
    }

    // Размер пера
    const int PEN_X = PAL_X + 8 * (SW + GAP) + 8;
    const int pens[3] = {1, 3, 5};
    const char* pen_labels[3] = {"1", "3", "5"};
    for (int i = 0; i < 3; i++) {
        bool active = (pen_size == pens[i]);
        // Подсвечиваем активный жёлтой рамкой перед кнопкой
        if (active) gfx_draw_rect(PEN_X + i * 22 - 1, 3, 22, 22, GFX_YELLOW);
        if (gui_button(PEN_X + i * 22, 4, 20, 20, pen_labels[i]))
            *new_pen = pens[i];
    }
    // Кнопка Clear
    if (gui_button(PEN_X + 70, 4, 40, 20, "Clear"))
        *clicked_clear = true;
}

void cmd_gui(const char* args) {
    (void)args;
    vga_enter_mode_640x480();

    int W = gfx_get_width(), H = gfx_get_height();
    int canvas_y = TOOLBAR_H;
    int canvas_h = H - TOOLBAR_H - STATUS_H;

    // Инициализация холста
    canvas_clear(canvas_h, GFX_WHITE);

    // Курсор
    gfx_cursor_move(W / 2, H / 2);
    gfx_cursor_show();

    static int  s_phys_x = 0, s_phys_y = 0;
    s_phys_x = mouse_state.x; s_phys_y = mouse_state.y;

    uint8_t cur_color  = GFX_BLACK;
    int     pen_size   = 3;
    int     prev_draw_x = -1, prev_draw_y = -1;
    bool    was_drawing = false;

    bool need_redraw = true;

    while (true) {
        uint8_t key = get_key();
        if (key == 27) break; // Esc

        gui_begin_frame(key);

        // Дельта курсора
        int cur_phys_x = mouse_state.x, cur_phys_y = mouse_state.y;
        int dx = cur_phys_x - s_phys_x, dy = cur_phys_y - s_phys_y;
        s_phys_x = cur_phys_x; s_phys_y = cur_phys_y;

        bool cursor_moved = false;
        if (dx != 0 || dy != 0) {
            int nx = gfx_cursor_get_x() + dx;
            int ny = gfx_cursor_get_y() + dy;
            if (nx < 0) nx = 0; if (nx >= W) nx = W - 1;
            if (ny < 0) ny = 0; if (ny >= H) ny = H - 1;
            gfx_cursor_move(nx, ny);
            cursor_moved = true;
        }

        int cx = gfx_cursor_get_x(), cy = gfx_cursor_get_y();

        // Рисование на холсте (LMB)
        bool lmb   = (mouse_state.buttons & 0x01) != 0;
        bool rmb   = (mouse_state.buttons & 0x02) != 0;
        bool in_canvas = (cx >= 0 && cx < W && cy >= canvas_y &&
                          cy < canvas_y + canvas_h);
        bool drew = false;

        if ((lmb || rmb) && in_canvas) {
            uint8_t draw_color = rmb ? GFX_WHITE : cur_color;
            int     ccx = cx, ccy = cy - canvas_y;
            if (was_drawing && prev_draw_x >= 0) {
                canvas_line(prev_draw_x, prev_draw_y,
                            ccx, ccy, canvas_h, pen_size, draw_color);
            } else {
                canvas_draw(ccx, ccy, canvas_h, pen_size, draw_color);
            }
            prev_draw_x = ccx; prev_draw_y = ccy;
            was_drawing = true;
            drew = true;
            need_redraw = true;
        } else {
            was_drawing = false;
            prev_draw_x = -1; prev_draw_y = -1;
        }

        if (need_redraw || cursor_moved || drew) {
            need_redraw = false;

            // Toolbar
            bool clear_clicked = false;
            uint8_t new_color = cur_color;
            int new_pen = pen_size;
            draw_toolbar(W, cur_color, pen_size,
                         &clear_clicked, &new_color, &new_pen);
            if (clear_clicked) {
                canvas_clear(canvas_h, GFX_WHITE);
            }
            cur_color = new_color;
            if (new_pen != pen_size) pen_size = new_pen;

            // Холст
            blit_canvas(W, canvas_y, canvas_h);

            // Статус
            gfx_fill_rect(0, H - STATUS_H, W, STATUS_H, GFX_BLACK);
            gfx_draw_text(4, H - STATUS_H + 2,
                          "LMB=Draw  RMB=Erase  Esc=Exit",
                          GFX_LIGHT_GRAY, GFX_BLACK);

            gfx_flip();
        }
    }

    gfx_cursor_hide();
    vga_exit_to_text();
    shell_clear_buffer();
    shell_redraw_full();
}
