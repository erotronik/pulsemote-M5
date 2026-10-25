#include "lvgl-utils.h"

#if CONFIG_BOARD_M5TAB5

#include <M5Unified.h>
#include <esp_heap_caps.h>
constexpr int32_t SCREENW = 1280;
constexpr int32_t SCREENH = 720;

lv_display_t *display;
lv_indev_t *indev;

// Without a battery fitted say full, not empty. Tell that from a flat battery (which reads 0% too, and
// charges) by its voltage: a flat 2S battery still has several volts
int hardware_get_battery_level() {
  if (M5.Power.getBatteryVoltage() < 1000) return 100;
  return M5.Power.getBatteryLevel();
}

bool hardware_is_charging() {
    return M5.Power.isCharging();
}

// The speaker is only on while beeping: left on, its amplifier crackles and whines with whatever
// else the board is doing. hardware_tft_loop() turns it off again
void hardware_beep(int duration_ms) {
  if (!M5.Speaker.isRunning()) M5.Speaker.begin();
  M5.Speaker.tone(2000, duration_ms);
}

// Full brightness is more than it needs, and the most power it draws: on a PC's USB port it browned out
constexpr uint8_t brightness_normal = 128;
constexpr uint8_t brightness_dimmed = 32;

void hardware_set_dimmed(bool dimmed) {
  M5.Display.setBrightness(dimmed ? brightness_dimmed : brightness_normal);
}

void hardware_tft_loop() {
    M5.update();
    if (M5.Speaker.isRunning() && !M5.Speaker.isPlaying()) M5.Speaker.end();
}

void hardware_tft_init() {
  ESP_LOGD("main", "M5.begin()");
  M5.begin();
  M5.Display.setBrightness(brightness_normal); // needed since M5 update after Apr 2026
  M5.Speaker.begin();  // and straight off again, so the amplifier is off until the first beep
  M5.Speaker.end();

  M5.Display.setRotation(3);
  M5.Display.setSwapBytes(true);

  ESP_LOGD("main", "lv_init()");
  lv_init();
  // LVGL's own 64k pool runs out quickly at the Tab5's scale (opening the ZC95's pattern list hung it):
  // give it more from PSRAM. Each pool has to be smaller than LV_MEM_SIZE, the largest LVGL's allocator
  // can handle
  constexpr size_t lv_extra_pool = 60 * 1024;
  for (int i = 0; i < 8; i++) {
    void *pool = heap_caps_malloc(lv_extra_pool, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pool || !lv_mem_add_pool(pool, lv_extra_pool))
      ESP_LOGE("board", "couldn't add LVGL memory pool %d", i);
  }
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
