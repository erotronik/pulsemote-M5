#pragma once
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include <cstdio>
#include <Arduino.h>

constexpr int32_t SCREENW = 320;
constexpr int32_t SCREENH = 240;

constexpr int32_t dropdown_height = 40;
constexpr int32_t dropdown_width = 160;

void m5io_showanalogrgb(byte sw, const lv_color_t rgb);

int lv_get_tabview_idx_from_page(lv_obj_t *l, lv_obj_t *page);
void lvgl_display_flush(lv_display_t *disp, const lv_area_t *area,uint8_t *px_map);
uint32_t lvgl_tick_function(void);
void lvgl_touchpad_read(lv_indev_t *drv, lv_indev_data_t *data);
void lv_hide_tab(lv_obj_t *page);
void printf_log(const char *format, ...);