#ifdef BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_7

#include <esp_display_panel.hpp>

#define GPIO_INPUT_IO_4     4

#include "lvgl-utils.h"

lv_display_t *display;
lv_indev_t *indev;

uint32_t lvgl_tick_function() { return (esp_timer_get_time() / 1000LL); }

esp_panel::board::Board   *panel = nullptr;
esp_panel::drivers::LCD   *lcd   = nullptr;

static const uint32_t screenWidth = 800;
static const uint32_t screenHeight = 480;
//int buf_size_in_bytes can be divided if doing a partial refresh, /8 or etc
static const int buf_size_in_bytes = screenWidth * screenHeight * sizeof(lv_color_t) /10; // 10 to 25% waveshare doc
static lv_color_t *disp_draw_buf, *disp_draw_buf2 = NULL;
//static uint16_t disp_draw_buf[buf_size_in_bytes / sizeof(lv_color_t)];

void lvgl_display_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  if (lcd) lcd->drawBitmap(area->x1,area->y1,w,h,(const uint8_t *)px_map);
  lv_disp_flush_ready(disp);
}

void lvgl_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data)
{
    auto touch = panel ? panel->getTouch() : nullptr;
    if (!touch) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }
    esp_panel::drivers::TouchPoint points[5];

    touch->readRawData(-1,0,0);
    int count = touch->getPoints(points, 5);

    if (count > 0) {
        // Use the first touch for LVGL pointer
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = points[0].x;
        data->point.y = points[0].y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void hardware_tft_loop() {
  lv_timer_handler(); 
}

void hardware_tft_init() {
  pinMode(GPIO_INPUT_IO_4, OUTPUT); // not sure if esp_panel takes care of this

  panel = new esp_panel::board::Board();
  panel->init(); 
  panel->begin();
  lcd = panel->getLCD();
  auto backlight = panel->getBacklight();
  backlight->on();

  lv_init();
  lv_tick_set_cb(lvgl_tick_function);
  display = lv_display_create(screenWidth, screenHeight);
  lv_display_set_flush_cb(display, lvgl_display_flush);

  disp_draw_buf = (lv_color_t *) heap_caps_malloc(buf_size_in_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  disp_draw_buf2 = (lv_color_t *) heap_caps_malloc(buf_size_in_bytes, MALLOC_CAP_SPIRAM |  MALLOC_CAP_8BIT);

  if (disp_draw_buf == nullptr) {
    while (1)    ESP_LOGE("waveshare","LVGL disp_draw_buf allocate failed!");
  }
  lv_display_set_buffers(display, disp_draw_buf, disp_draw_buf2, buf_size_in_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, lvgl_touchpad_read);

  lv_init_pulsemote();


}



#endif