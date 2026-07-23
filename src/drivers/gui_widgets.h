#ifndef GUI_WIDGETS_H
#define GUI_WIDGETS_H

#include <stdint.h>
#include <stdbool.h>

// ─── Один вызов в начале каждой итерации цикла ───────────────────────────────
void gui_begin_frame(uint8_t key);

// ─── Базовые виджеты ─────────────────────────────────────────────────────────
bool gui_button   (int x, int y, int w, int h, const char* label);
bool gui_checkbox (int x, int y, const char* label, bool* checked);
bool gui_textfield(int x, int y, int w, char* buf, int buf_size, bool* focused);
bool gui_slider   (int x, int y, int w, float* value);
bool gui_toggle   (int x, int y, const char* label, bool* checked);

// ─── Прогресс-бар (только отображение, не интерактивный) ─────────────────────
// value: 0.0..1.0  |  color: цвет заполненной части (GFX_LIGHT_BLUE, LIGHT_GREEN…)
void gui_progressbar(int x, int y, int w, float value, uint8_t color);

// ─── Контекстное меню ────────────────────────────────────────────────────────
bool gui_context_menu(int x, int y, const char** items, int item_count, int* out_index);
void gui_context_menu_close();
bool gui_context_menu_is_open();

// ─── Текстовая скролл-панель ──────────────────────────────────────────────────
// lines/line_count  — массив строк
// scroll_y          — вертикальный скролл (в строках)
// scroll_x          — горизонтальный скролл (в символах)
// Возвращает true если что-то изменилось
bool gui_scroll_panel(int x, int y, int w, int panel_h,
                      const char** lines, int line_count,
                      int* scroll_y, int* scroll_x);

// ─── Скролл-панель с произвольным контентом (виджеты внутри) ─────────────────
// Пример использования:
//   static int sv = 0;
//   int cy = gui_scroll_begin(10, 50, 160, 100, 300, &sv);
//   gui_button(14, cy + 4,  80, 16, "Кнопка 1");   // cy+4 — смещение контента
//   gui_button(14, cy + 24, 80, 16, "Кнопка 2");
//   gui_scroll_end(10, 50, 160, 100, 300, sv);
//
// gui_scroll_begin активирует clip-rect, возвращает Y начала контента
// (panel_y + 1 - scroll_y_pixels). Рисуй виджеты с этим Y-офсетом.
// gui_scroll_end сбрасывает clip и рисует скроллбар.
int  gui_scroll_begin(int x, int y, int w, int h,
                      int content_total_h, int* scroll_y);
void gui_scroll_end  (int x, int y, int w, int h,
                      int content_total_h, int scroll_y);

// ─── Вспомогательные ─────────────────────────────────────────────────────────
bool gui_mouse_down();
bool gui_click_edge();
bool gui_release_edge();
bool gui_point_in_rect(int px, int py, int x, int y, int w, int h);

#endif // GUI_WIDGETS_H
