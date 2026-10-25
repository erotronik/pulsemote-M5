#include "lvgl-utils.h"

#if CONFIG_BOARD_M5CORES3

#include <M5Unified.h>
#include <esp_heap_caps.h>

constexpr int32_t SCREENW = 320;
constexpr int32_t SCREENH = 240;

lv_display_t *display;
lv_indev_t *indev;

int hardware_get_battery_level() {
    return M5.Power.getBatteryLevel();
}

bool hardware_is_charging() {
    return M5.Power.isCharging();
}

void hardware_beep(int duration_ms) {
  M5.Speaker.tone(2000, duration_ms);
}

// The backlight runs from the AXP2101's DLDO1, which M5GFX sets from 2.5V (brightness 1) to 3.3V
// (255) in steps of 32. Full brightness runs the battery down quickly
constexpr uint8_t brightness_normal = 160;  // 3.0V
constexpr uint8_t brightness_dimmed = 64;   // 2.7V: 2.5V is too dark to read

void hardware_set_dimmed(bool dimmed) {
  M5.Display.setBrightness(dimmed ? brightness_dimmed : brightness_normal);
}

void hardware_tft_loop() {
    M5.update();
}

void hardware_tft_init() {
  M5.begin();
  M5.Display.setBrightness(brightness_normal); // needed since M5 update after Apr 2026
  M5.Speaker.begin();
  lv_init();
  // LVGL's own 64k pool only holds a few device tabs: give it more from PSRAM. Each pool has to be
  // smaller than LV_MEM_SIZE, the largest LVGL's allocator can handle
  constexpr size_t lv_extra_pool = 60 * 1024;
  for (int i = 0; i < 4; i++) {
    void *pool = heap_caps_malloc(lv_extra_pool, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pool || !lv_mem_add_pool(pool, lv_extra_pool))
      ESP_LOGE("board", "couldn't add LVGL memory pool %d", i);
  }
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
