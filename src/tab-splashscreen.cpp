#include "M5Unified.h"
#include <memory>
#define LV_CONF_INCLUDE_SIMPLE
#include <hsv.h>
#include <lvgl.h>

#include "tab-object-timer.hpp"
#include "tab-splashscreen.hpp"
#include "tab.hpp"

tab_splashscreen::tab_splashscreen() {
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
  type = DeviceType::splashscreen;
};
tab_splashscreen::~tab_splashscreen(){};

void m5io_showanalogrgb(byte sw, const CRGB &rgb);
void dump_connected_devices(void);

lv_obj_t *lv_debug_window;
byte buttonhue[] = {0, 64, 128, 192, 0};

void tab_splashscreen::loop(boolean activetab) {
  if (activetab) {
    for (int i = 0; i < 4; i++) {
      m5io_showanalogrgb(i + 1,
                         hsvToRgb(buttonhue[i], 255, 128));  // rotary LED
      buttonhue[i]++;
    }
    m5io_showanalogrgb(
        5, hsvToRgb(255, 0, buttonhue[4] / 2));  // cherry LED (very bright)
    buttonhue[4]++;
  }
}

tab_object_timer *ttest;


void tab_splashscreen::switch_change(int sw, boolean value) {
  ESP_LOGI("splashscreen", "new callback button %d %s\n", sw,
           value ? "push" : "release");
  if (sw == 4 && value) {
    printf_log("Battery %dmV level %d%%\n", M5.Power.getBatteryVoltage(), M5.Power.getBatteryLevel());
    //printf_log("%d=%d\n",M5.getBoard(),lgfx::boards::board_M5StackCoreS3);
    dump_connected_devices();
  }
  if (sw == 3 && value && ttest != nullptr) {
    ttest->highlight_next_field();
  }
  if (value) buttonhue[sw] = 0;
}

void tab_splashscreen::encoder_change(int sw, int change) {
  ESP_LOGI("splashscreen", "Encoder %d: %+d\n", sw, change);
  if (sw == 3 && ttest != nullptr) ttest->rotary_change(change);
}

void tab_splashscreen::setup(void) {
  lv_obj_t *tv2 = lv_tabview_add_tab(tv, "Pulsemote");

  lv_obj_set_style_pad_left(tv2, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_right(tv2, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(tv2, 0, LV_PART_MAIN);

  // always first child
  lv_debug_window = lv_textarea_create(tv2);
  lv_textarea_add_text(lv_debug_window, "");
  lv_textarea_set_cursor_click_pos(lv_debug_window, false);
  lv_obj_set_size(lv_debug_window, lv_pct(100), lv_pct(60));
  lv_obj_set_align(lv_debug_window, LV_ALIGN_BOTTOM_LEFT);

  ttest = nullptr;
  // t = new tab_object_timer(false); //just for testing!
  // t->view(tv2);

  page = tv2;
}