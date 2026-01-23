#include "lvgl-utils.h"

#if CONFIG_BOARD_M5TAB5

#include <M5Unified.h>
constexpr int32_t SCREENW = 1280;
constexpr int32_t SCREENH = 720;

lv_display_t *display;
lv_indev_t *indev;

int hardware_get_battery_level() {
  if (M5.Power.getBatteryLevel() == 0 && M5.Power.isCharging()) return 100; // no batter
  return M5.Power.getBatteryLevel();
}

bool hardware_is_charging() {
    return M5.Power.isCharging();
}

void hardware_beep() {
  M5.Speaker.tone(2000, 1000);
}

void hardware_tft_loop() {
    M5.update();
}

void hardware_tft_init() {
  ESP_LOGD("main", "M5.begin()");
  M5.begin();
  M5.Speaker.begin();

  M5.Display.setRotation(3);
  M5.Display.setSwapBytes(true);

  ESP_LOGD("main", "lv_init()");
  lv_init();
  ESP_LOGD("main", "lv_tick_set_cb()");
  lv_tick_set_cb(lvgl_tick_function);
  ESP_LOGD("main", "lv_display_create()");
  display = lv_display_create(M5.Display.width(), M5.Display.height());
  ESP_LOGD("main", "lv_display_set_flush_cb()");
  lv_display_set_flush_cb(display, lvgl_display_flush);
  ESP_LOGD("main","allocate buffers");
  const uint32_t buf_pixels = M5.Display.width() * 40;
  static lv_color_t *buf1 = (lv_color_t*) heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
  static lv_color_t *buf2 = (lv_color_t*) heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
  ESP_LOGD("main", "lv_display_set_buffers()");
  lv_display_set_buffers(display, buf1, buf2, buf_pixels * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
  ESP_LOGD("main", "lv_indev_create()");
  indev = lv_indev_create();
  ESP_LOGD("main", "lv_indev_set_type()");
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  ESP_LOGD("main", "lv_indev_set_read_cb()");
  lv_indev_set_read_cb(indev, lvgl_touchpad_read);
  ESP_LOGD("main", "lv_init_pulsemote()");
  lv_init_pulsemote();
}

void lvgl_display_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  //lv_draw_sw_rgb565_swap(px_map, w * h);
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
