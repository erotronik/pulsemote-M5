#pragma once

#include <lvgl.h>   // for lv_color_t
#include <stdint.h>
#include <queue.h>

void m5io_showanalogrgb(uint8_t sw, lv_color_t rgb);
void m5io_init(void);
const uint8_t numencoders = 4;

typedef struct {
  int target;
  int value;
} event_t;

extern QueueHandle_t event_queue;
