#include <lvgl-utils.h>

void hardware_tft_init(void);
void hardware_tft_loop(void);
extern lv_display_t *display;
extern lv_indev_t *indev;

esp_err_t hardware_temp_write_0x41(uint8_t *data);
uint8_t hardware_temp_read_0x41(uint8_t reg);
