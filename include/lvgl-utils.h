#pragma once
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include <cstdio>

constexpr int32_t SCREENW = 320;
constexpr int32_t SCREENH = 240;

constexpr int32_t dropdown_height = 40;
constexpr int32_t dropdown_width = 160;

extern lv_style_t lvpulsemote_style_status;
extern lv_style_t lvpulsemote_style_tab;
extern lv_style_t lvpulsemote_style_checked;

int lv_get_tabview_idx_from_page(lv_obj_t *l, lv_obj_t *page);
void lvgl_display_flush(lv_display_t *disp, const lv_area_t *area,uint8_t *px_map);
uint32_t lvgl_tick_function(void);
void lvgl_touchpad_read(lv_indev_t *drv, lv_indev_data_t *data);
void lv_hide_tab(lv_obj_t *page);
void printf_log(const char *format, ...);
void lv_init_pulsemote(void);

//#include "esp_timer.h"
//
//static inline uint32_t millis(){
//    return (uint32_t)(esp_timer_get_time() / 1000ULL);
//}