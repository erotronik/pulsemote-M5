#pragma once

#include <lvgl.h>   // for lv_color_t
#include <stdint.h>
#include <freertos/queue.h>

void pulsemote_pcb_setleds(uint8_t sw, lv_color_t rgb);
void pulsemote_pcb_init(void);
const uint8_t numencoders = 4;

typedef struct {
  int target;
  int value;
} event_t;

extern QueueHandle_t event_queue;

#if defined(ARDUINO)
#include <Arduino.h>
#else
// ESP-IDF compatibility: provide millis() wrapper
#include <esp_timer.h>

unsigned long millis(void);
#define HIGH 1
#define CHANGE 1 // not used
#endif
