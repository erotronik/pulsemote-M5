#pragma once

void hardware_tft_loop();
void hardware_tft_init();
int hardware_get_battery_level();
bool hardware_is_charging();
void hardware_beep();

extern lv_display_t *display;
extern lv_indev_t *indev;