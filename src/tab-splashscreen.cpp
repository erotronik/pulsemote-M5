#include "M5Unified.h"
#include <memory>
#define LV_CONF_INCLUDE_SIMPLE
#include <hsv.h>
#include <lvgl.h>

#include "tab-object-timer.hpp"
#include "tab-splashscreen.hpp"
#include "tab.hpp"
#include "lvgl-utils.h"

// just a test mode to display what we think all the connected
// devices are to the debug window when you push a button
// on the splashscreen

void dump_connected_devices(void) {
  for (int i = 0; i < tabs.size(); i++) {
    Tab *t = tabs.get(i);
    //if (t->page != nullptr) {
    //  printf_log("tab=%d ", lv_get_tabview_idx_from_page(tv, t->page));
    //}
    if (t->device != nullptr) {
      printf_log("%s: ", t->device->getShortName());
    } else {
        printf_log("tab %d: ", i);
    }
    printf_log("count=%d\n",t->getcyclecount());
  }
}

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

void tab_splashscreen::updateicons() {
  int level = min(4,M5.Power.getBatteryLevel() / 20);
  lv_label_set_text_fmt(labelicons, "%s%s",batteryicons[level], M5.Power.isCharging()?batteryicons[5]:"");
}

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
  if (batterycheckmillis == 0 || (millis() - batterycheckmillis) > 20000) { // every 20 sec
    updateicons();
    batterycheckmillis = millis();
  }
}

void tab_splashscreen::switch_change(int sw, boolean value) {
  ESP_LOGI("splashscreen", "new callback button %d %s\n", sw,
           value ? "push" : "release");
  if (sw == 4 && value) {
    dump_connected_devices();
  }
  if (value) buttonhue[sw] = 0;
}

void tab_splashscreen::encoder_change(int sw, int change) {
  ESP_LOGI("splashscreen", "Encoder %d: %+d\n", sw, change);
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

  labelicons = lv_label_create(tv2);
  lv_label_set_text(labelicons, "");
  lv_obj_set_style_text_align(labelicons, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(labelicons, LV_ALIGN_TOP_RIGHT, -8, 0);
  lv_obj_set_style_text_font(labelicons, &lv_font_montserrat_24, LV_PART_MAIN);

  page = tv2;
}