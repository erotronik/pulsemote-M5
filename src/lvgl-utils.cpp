#define LV_CONF_INCLUDE_SIMPLE
#include <esp_timer.h>
#include <lvgl.h>
#include <M5Unified.h>
#include "lvgl-utils.h"
#include <tab.hpp>

// Log to serial, and if the splashscreen is there, also to the debug window of the splashscreen

void printf_log(const char *format, ...) {
  static char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, 255, format, args);
  va_end(args);
  Serial.print(buf);
  if (tabs.size() > 0) {
    Tab *t = tabs.front();
    if (t->page) {
      lv_obj_t *child = lv_obj_get_child(t->page, 0);
      lv_textarea_add_text(child, buf);
    }
  }
}

// given a tabview and a content page of a tab, get the tab index. This
// stops us having to keep track of the tabid which changes when we
// delete tabs as hardware goes away

int lv_get_tabview_idx_from_page(lv_obj_t *l, lv_obj_t *page) {
  lv_obj_t *c = lv_tabview_get_content(l);
  for (int i = 0; i < lv_obj_get_child_count(c); i++) {
    if (lv_obj_get_child(c, i) == page) return i;
  }
  return -1;
}

void lv_hide_tab(lv_obj_t *page) {
  if (!page) return;
  int tabid = lv_get_tabview_idx_from_page(tv, page);
  ESP_LOGD("main", "hide tab id %d", tabid);
  if (tabid == -1) return;
  // if we're viewing the tab that's gone away then switch to the main screen
  if (lv_tabview_get_tab_act(tv) == tabid)
    lv_tabview_set_act(tv, 0, LV_ANIM_OFF);

  lv_obj_t *tbar = lv_tabview_get_tab_bar(
      tv);  // in lvgl 9 they are real buttons not a matrix
  lv_obj_t *cont = lv_tabview_get_content(tv);
  lv_obj_del(lv_obj_get_child(tbar, tabid));
  lv_obj_del(lv_obj_get_child(cont, tabid));
}

void lvgl_display_flush(lv_display_t *disp, const lv_area_t *area,
                      uint8_t *px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  lv_draw_sw_rgb565_swap(px_map, w * h);
  M5.Display.pushImageDMA<uint16_t>(area->x1, area->y1, w, h,
                                    (uint16_t *)px_map);
  lv_disp_flush_ready(disp);
}

uint32_t lvgl_tick_function() { return (esp_timer_get_time() / 1000LL); }

void lvgl_touchpad_read(lv_indev_t *drv, lv_indev_data_t *data) {
  M5.update();
  auto count = M5.Touch.getCount();

  if (count == 0) {
    data->state = LV_INDEV_STATE_RELEASED;
  } else {
    auto touch = M5.Touch.getDetail(0);
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = touch.x;
    data->point.y = touch.y;
  }
}