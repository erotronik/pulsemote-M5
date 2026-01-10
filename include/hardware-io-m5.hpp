#include "lvgl-utils.h"

#pragma once
void m5io_showanalogrgb(byte sw, lv_color_t rgb);
void m5io_init(void);
const byte numencoders = 4;

typedef struct {
  int target;
  int value;
} event_t;

extern QueueHandle_t event_queue;
