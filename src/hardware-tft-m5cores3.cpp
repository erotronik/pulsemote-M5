#ifdef M5_BOARD

#include <M5Unified.h>
#include "lvgl-utils.h"

lv_display_t *display;
lv_indev_t *indev;

void hardware_tft_loop() {
    M5.update();
}

void hardware_tft_init() {
  M5.begin();
  lv_init();
  lv_tick_set_cb(lvgl_tick_function);
  display = lv_display_create(SCREENW, SCREENH);
  lv_display_set_flush_cb(display, lvgl_display_flush);
  static lv_color_t buf1[SCREENW * 15];
  lv_display_set_buffers(display, buf1, nullptr, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, lvgl_touchpad_read);
  lv_init_pulsemote();

}

void lvgl_display_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  lv_draw_sw_rgb565_swap(px_map, w * h);
  M5.Display.pushImageDMA<uint16_t>(area->x1, area->y1, w, h, (uint16_t *)px_map);
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

#endif