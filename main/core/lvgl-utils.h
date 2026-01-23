#pragma once
#include <lvgl.h>
#include <cstdio>

constexpr int32_t dropdown_height = 40;
constexpr int32_t dropdown_width = 160;

extern lv_style_t lvpulsemote_style_status;
extern lv_style_t lvpulsemote_style_tab;
extern lv_style_t lvpulsemote_style_checked;

extern float _lv_scale;
#define LV_SCALE(x) ((int32_t)((x) * _lv_scale))
const lv_font_t* lv_font_get_scaled(uint32_t size);
#define LV_FONT_GET(size) lv_font_get_scaled(size)

int lv_get_tabview_idx_from_page(lv_obj_t *l, lv_obj_t *page);
void lvgl_display_flush(lv_display_t *disp, const lv_area_t *area,uint8_t *px_map);
uint32_t lvgl_tick_function(void);
void lvgl_touchpad_read(lv_indev_t *drv, lv_indev_data_t *data);
void lv_hide_tab(lv_obj_t *page);
void printf_log(const char *format, ...);
void lv_init_pulsemote(void);

// ESP-IDF compatibility: provide millis() wrapper
#include <esp_timer.h>

// Arduino compatibility wrapper for ESP-IDF
// Returns milliseconds since boot (wraps after ~49 days like Arduino)
inline unsigned long millis() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}
