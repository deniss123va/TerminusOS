#include "cmd_widgets.h"
#include "../drivers/vga_gfx.h"
#include "../drivers/gui_widgets.h"
#include "../kernel/keyboard.h"
#include "../kernel/mouse.h"
#include "../lib/screen.h"
#include "../shell/shell.h"
#include "../drivers/serial.h"

static const int SB_INNER_W = 12; // отступ для скроллбара в панели виджетов

// Демо-строки для скроллируемой панели
static const char* demo_lines[] = {
    "Welcome to TerminusOS!",
    "========================",
    "",
    "This is a scrollable panel.",
    "You can scroll with arrow keys",
    "or middle mouse button.",
    "",
    "Features:",
    "  - Sliders with drag",
    "  - Toggle switches",
    "  - Context menus",
    "  - Scrollable panels",
    "",
    "The OS is written in C++",
    "and runs on bare metal.",
    "",
    "Keyboard shortcuts:",
    "  Esc = exit",
    "  Arrows = scroll panel",
    "",
    "Mouse controls:",
    "  Left click = interact",
    "  Middle click + move = scroll",
    "  Right click = context menu",
    "",
    "Enjoy the demo!",
    "========================",
    "Line 26: More content...",
    "Line 27: Even more...",
    "Line 28: Keep scrolling...",
    "Line 29: Almost there...",
    "Line 30: The end!",
};
static const int DEMO_LINE_COUNT = sizeof(demo_lines) / sizeof(demo_lines[0]);

// Пункты контекстного меню
static const char* menu_items[] = {
    "Copy",
    "Paste",
    "Cut",
    "---",
    "Settings",
    "About",
    "Exit",
};
static const int MENU_ITEM_COUNT = sizeof(menu_items) / sizeof(menu_items[0]);

void cmd_widgets(const char* args) {
    serial_write("[WGT] enter\n");
    bool mode640 = (args && args[0] == '6');
    if (mode640) vga_enter_mode_640x480();
    else         vga_enter_mode13();
    serial_write("[WGT] mode ok\n");

    int W = gfx_get_width(), H = gfx_get_height();
    serial_write("[WGT] W="); serial_dec(W);
    serial_write(" H="); serial_dec(H); serial_write("\n");

    static char name_buf[32];
    static bool name_focused;
    static bool agree;
    static bool dark_mode;
    static int  click_count;
    static char echo_buf[40];
    static float slider_val;
    static int scroll_offset;
    static int scroll_x;       // горизонтальный скролл
    static int widget_scroll;  // скролл для scroll_begin/end панели
    static bool menu_open;
    static int menu_sel;
    static char menu_status[40];

    // Обнулить статики при каждом запуске команды
    for (int i = 0; i < 32; i++) name_buf[i] = 0;
    name_focused  = false;
    agree         = false;
    dark_mode     = false;
    click_count   = 0;
    slider_val    = 0.5f;
    scroll_offset = 0;
    scroll_x      = 0;
    widget_scroll = 0;
    menu_open     = false;
    menu_sel      = -1;
    for (int i = 0; i < 40; i++) { echo_buf[i] = 0; menu_status[i] = 0; }

    // Инициализируем курсор
    gfx_cursor_move(mouse_state.x, mouse_state.y);
    gfx_cursor_show();

    // --- Дельта-трекинг: курсор двигается на дельту, не прыгает ---
    static int  s_phys_x = 0, s_phys_y = 0;    // физ. позиция мыши прошлого кадра
    static bool s_mmb_was       = false;         // СКМ была зажата
    static bool s_dir_locked    = false;         // направление скролла зафиксировано
    static bool s_dir_horiz     = false;         // true=горизонталь, false=вертикаль
    static int  s_drag_total_x  = 0;             // накопленная дельта X с начала СКМ
    static int  s_drag_total_y  = 0;             // накопленная дельта Y с начала СКМ
    static int  s_scroll_sub_x  = 0;             // суб-пиксельный аккумулятор X
    static int  s_scroll_sub_y  = 0;             // суб-пиксельный аккумулятор Y

    // Сбрасываем при каждом запуске команды
    s_phys_x = mouse_state.x; s_phys_y = mouse_state.y;
    s_mmb_was = false; s_dir_locked = false; s_dir_horiz = false;
    s_drag_total_x = s_drag_total_y = 0;
    s_scroll_sub_x = s_scroll_sub_y = 0;

    serial_write("[WGT] entering loop\n");

    // Первичная отрисовка
    gfx_clear(GFX_BLUE);
    serial_write("[WGT] first clear ok\n");
    gfx_flip();
    serial_write("[WGT] first flip ok\n");

    uint8_t key = 0;
    bool need_redraw = true;

    while (true) {
        key = get_key();
        if (key == 27) break; // Esc

        gui_begin_frame(key);

        // --- Физическая дельта мыши ---
        // Для движения курсора нужна клампнутая позиция (mouse_state.x/y),
        // но для СКМ-скролла берём raw_dx/raw_dy из последнего пакета —
        // diff клампнутых координат даёт 0 на краю экрана и скролл "залипает"
        // (см. mouse.h). Обычное перемещение курсора это не затрагивает.
        int cur_phys_x = mouse_state.x, cur_phys_y = mouse_state.y;
        int dphys_x_cursor = cur_phys_x - s_phys_x;
        int dphys_y_cursor = cur_phys_y - s_phys_y;
        s_phys_x = cur_phys_x; s_phys_y = cur_phys_y;

        bool mmb_now  = (mouse_state.buttons & 0x04) != 0;
        // Во время скролла (MMB) используем сырую, неклампленную дельту;
        // вне скролла — обычную дельту курсора (идентична предыдущему поведению)
        int dphys_x = mmb_now ? mouse_state.raw_dx : dphys_x_cursor;
        int dphys_y = mmb_now ? mouse_state.raw_dy : dphys_y_cursor;
        bool mmb_press = mmb_now && !s_mmb_was;   // СКМ только что нажата
        s_mmb_was = mmb_now;

        if (mmb_press) {
            // Сброс состояния при новом нажатии СКМ
            s_dir_locked    = false;
            s_drag_total_x  = 0; s_drag_total_y  = 0;
            s_scroll_sub_x  = 0; s_scroll_sub_y  = 0;
        }

        bool cursor_moved = false;

        if (mmb_now) {
            // Курсор заморожен — двигаем только при наличии дельты
            gfx_cursor_set_locked(true);

            if (dphys_x != 0 || dphys_y != 0) {
                s_drag_total_x += dphys_x;
                s_drag_total_y += dphys_y;

                int ax = s_drag_total_x < 0 ? -s_drag_total_x : s_drag_total_x;
                int ay = s_drag_total_y < 0 ? -s_drag_total_y : s_drag_total_y;

                // Фиксируем направление после 5px движения
                if (!s_dir_locked && (ax >= 5 || ay >= 5)) {
                    s_dir_locked = true;
                    s_dir_horiz  = (ax >= ay);
                }

                if (s_dir_locked) {
                    if (s_dir_horiz) {
                        // Горизонтальный скролл (8px = 1 символ)
                        // Мёртвая зона: вертикаль игнорируется полностью
                        s_scroll_sub_x += dphys_x;
                        int ch = s_scroll_sub_x / 8;
                        if (ch != 0) {
                            scroll_x += ch;
                            if (scroll_x < 0) scroll_x = 0;
                            s_scroll_sub_x -= ch * 8;
                            need_redraw = true;
                        }
                    } else {
                        // Вертикальный скролл (10px = 1 строка)
                        // Мёртвая зона: горизонталь игнорируется полностью
                        s_scroll_sub_y += dphys_y;
                        int ln = s_scroll_sub_y / 10;
                        if (ln != 0) {
                            scroll_offset += ln;
                            if (scroll_offset < 0) scroll_offset = 0;
                            if (scroll_offset > DEMO_LINE_COUNT - 1)
                                scroll_offset = DEMO_LINE_COUNT - 1;
                            s_scroll_sub_y -= ln * 10;
                            need_redraw = true;
                        }
                    }
                }
            }
        } else {
            // СКМ не зажата: двигаем курсор на дельту (не прыгает)
            gfx_cursor_set_locked(false);
            if (dphys_x != 0 || dphys_y != 0) {
                int old_cx = gfx_cursor_get_x(), old_cy = gfx_cursor_get_y();
                int new_cx = old_cx + dphys_x;
                int new_cy = old_cy + dphys_y;
                if (new_cx < 0) new_cx = 0; if (new_cx >= W) new_cx = W - 1;
                if (new_cy < 0) new_cy = 0; if (new_cy >= H) new_cy = H - 1;
                gfx_cursor_move(new_cx, new_cy);
                cursor_moved = (gfx_cursor_get_x() != old_cx ||
                                gfx_cursor_get_y() != old_cy);
            }
        }

        bool clicked = gui_click_edge();

        // Проверяем ПКМ
        static bool rmb_prev = false;
        bool rmb_now  = (mouse_state.buttons & 0x02) != 0;
        bool rmb_edge = rmb_now && !rmb_prev;
        rmb_prev = rmb_now;

        if (rmb_edge && !menu_open) {
            menu_open = true;
            menu_sel  = -1;
        }

        bool released = gui_release_edge();  // кнопка отпущена → нужен перерисов

        // ЛКМ зажата — нужен редрейв каждый кадр, иначе виджеты типа слайдера
        // не вызываются во время драга (гейт scene_changed их пропускал,
        // и *value обновлялся только в момент клика, а не при движении мыши).
        bool lmb_down = gui_mouse_down();

        bool scene_changed = (key || clicked || released || need_redraw ||
                              rmb_edge || menu_open || mmb_now || lmb_down);

        if (scene_changed || cursor_moved) {
            if (scene_changed) {
                need_redraw = false;

                gfx_clear(GFX_BLUE);
            gfx_draw_text(8, 6, "Widgets Demo v2.0 - Esc to exit", GFX_WHITE, GFX_BLUE);

            // ==================== ЛЕВАЯ КОЛОНКА ====================

            // --- Кнопка ---
            if (gui_button(8, 24, 90, 16, "Click me")) {
                click_count++;
                need_redraw = true;
            }
            char cbuf[16] = "clicks: ";
            int n = click_count, i = 8;
            if (n == 0) { cbuf[i++] = '0'; }
            else {
                char tmp[10]; int t = 0;
                while (n > 0) { tmp[t++] = (char)('0' + n % 10); n /= 10; }
                while (t > 0) cbuf[i++] = tmp[--t];
            }
            cbuf[i] = 0;
            gfx_draw_text(106, 28, cbuf, GFX_YELLOW, GFX_BLUE);

            // --- Чекбокс ---
            if (gui_checkbox(8, 50, "Enable feature", &agree)) need_redraw = true;
            gfx_draw_text(160, 51,
                          agree ? "ON " : "OFF",
                          agree ? GFX_LIGHT_GREEN : GFX_LIGHT_RED,
                          GFX_BLUE);

            // --- Текстовое поле ---
            gfx_draw_text(8, 74, "Name:", GFX_WHITE, GFX_BLUE);
            if (gui_textfield(50, 72, 120, name_buf, sizeof(name_buf), &name_focused)) {
                int j = 0;
                const char* p = "Hello, ";
                while (*p) echo_buf[j++] = *p++;
                const char* s = name_buf;
                while (*s && j < 38) echo_buf[j++] = *s++;
                echo_buf[j++] = '!'; echo_buf[j] = 0;
                need_redraw = true;
            }
            if (echo_buf[0])
                gfx_draw_text(8, 96, echo_buf, GFX_LIGHT_CYAN, GFX_BLUE);

            // --- Слайдер + Прогресс-бар ---
            gfx_draw_text(8, 116, "Volume:", GFX_WHITE, GFX_BLUE);
            if (gui_slider(8, 128, 140, &slider_val)) need_redraw = true;
            // Прогресс-бар под слайдером показывает то же значение
            gui_progressbar(8, 150, 140, slider_val, GFX_LIGHT_BLUE);

            // --- Тогл ---
            gfx_draw_text(8, 168, "Dark mode:", GFX_WHITE, GFX_BLUE);
            if (gui_toggle(80, 166, "", &dark_mode)) need_redraw = true;

            // ==================== ПРАВАЯ КОЛОНКА ====================
            int right_x = W / 2 + 4;
            if (right_x < 180) right_x = 180;

            // --- Скролл-панель с текстом (↑↓←→ для прокрутки) ---
            gfx_draw_text(right_x, 24, "Text Panel (arrows):", GFX_WHITE, GFX_BLUE);
            int panel_w = W - right_x - 8;
            if (panel_w < 80) panel_w = 80;
            int panel_h = (H - 90) / 2 - 4;
            if (panel_h < 40) panel_h = 40;
            if (gui_scroll_panel(right_x, 36, panel_w, panel_h,
                             demo_lines, DEMO_LINE_COUNT,
                             &scroll_offset, &scroll_x)) need_redraw = true;

            // --- Скролл-панель с виджетами (scroll_begin/end) ---
            int wp_y = 36 + panel_h + 14;
            gfx_draw_text(right_x, wp_y - 10, "Widget Panel:", GFX_WHITE, GFX_BLUE);
            int wp_h = H - wp_y - 8;
            if (wp_h < 40) wp_h = 40;
            const int CONTENT_H = 120;  // полная высота контента внутри
            int cy2 = gui_scroll_begin(right_x, wp_y, panel_w, wp_h,
                                       CONTENT_H, &widget_scroll);
            // Виджеты внутри — рисуются с cy2 как базой
            gui_button(right_x + 4, cy2 + 4,  80, 14, "Btn A");
            gui_button(right_x + 4, cy2 + 22, 80, 14, "Btn B");
            gui_button(right_x + 4, cy2 + 40, 80, 14, "Btn C");
            gui_progressbar(right_x+4, cy2+60, panel_w-SB_INNER_W, 0.33f, GFX_LIGHT_GREEN);
            gui_progressbar(right_x+4, cy2+76, panel_w-SB_INNER_W, 0.66f, GFX_YELLOW);
            gui_progressbar(right_x+4, cy2+92, panel_w-SB_INNER_W, 1.0f,  GFX_LIGHT_RED);
            gui_scroll_end(right_x, wp_y, panel_w, wp_h, CONTENT_H, widget_scroll);

            // --- Контекстное меню ---
            if (menu_open) {
                // БАГ БЫЛ ЗДЕСЬ: брали mouse_state.x/y — сырую позицию мыши,
                // которая продолжает копиться даже пока курсор "заморожен"
                // на время СКМ-скролла (см. gfx_cursor_set_locked выше). После
                // скролла она расходится с тем, что реально нарисовано на
                // экране, и меню открывается не под курсором. Видимая позиция
                // курсора — всегда gfx_cursor_get_x/y(), как и везде в файле.
                int mx = gfx_cursor_get_x();
                int my = gfx_cursor_get_y();
                if (mx + 80 > W) mx = W - 80;
                if (my + 120 > H) my = H - 120;

                if (gui_context_menu(mx, my, menu_items, MENU_ITEM_COUNT, &menu_sel)) {
                    menu_open = false;
                    need_redraw = true;
                    // Показываем статус выбора
                    const char* sel = menu_items[menu_sel];
                    int j = 0;
                    while (sel[j] && j < 38) { menu_status[j] = sel[j]; j++; }
                    menu_status[j] = 0;
                } else if (!gui_context_menu_is_open()) {
                    // LMB кликнули вне меню — закрылось внутри, синхронизируем флаг
                    menu_open = false;
                    need_redraw = true;
                }
            }

            // Показываем статус меню
            if (menu_status[0]) {
                gfx_draw_text(8, H - 28, "Menu selected: ", GFX_LIGHT_CYAN, GFX_BLUE);
                gfx_draw_text(8 + 15*8, H - 28, menu_status, GFX_YELLOW, GFX_BLUE);
            }

            // --- Статус-бар ---
            gfx_fill_rect(0, H - 11, W, 11, GFX_DARK_GRAY);
            gfx_draw_hline(0, H - 12, W, GFX_LIGHT_GRAY);
            gfx_draw_text(4, H - 10,
                          "LMB=click  Drag=slider  RMB=menu  Mid+move=scroll  Esc=exit",
                          GFX_LIGHT_GRAY, GFX_DARK_GRAY);

                gfx_flip();   // backbuffer → VRAM + курсор в новой позиции
            } else {
                // Только курсор двинулся — переотправляем готовый backbuffer
                // с курсором в новой позиции (без перерисовки виджетов)
                gfx_flip();
            }
        }
    }

    gfx_cursor_hide();
    vga_exit_to_text();
    shell_clear_buffer();
    shell_redraw_full();
}