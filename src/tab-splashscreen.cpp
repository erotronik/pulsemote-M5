#include "M5Unified.h"
#include <memory>
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>

#include "tab-object-timer.hpp"
#include "tab-splashscreen.hpp"
#include "tab.hpp"
#include "lvgl-utils.h"

tab_splashscreen::tab_splashscreen() {
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
  type = DeviceType::splashscreen;
};
tab_splashscreen::~tab_splashscreen(){};

// just a test mode to display what we think all the connected
// devices are to the debug window when you push a button
// on the splashscreen

void tab_splashscreen::dump_connected_devices(void) {
  int i=0;
  for (const auto& t: tabs) {
    printf_log("%i %s: ", i, t->gettabname());
    printf_log("count=%d\n",t->getcyclecount());
    i++;
  }
}

void tab_splashscreen::updateicons() {
  int level = min(4,M5.Power.getBatteryLevel() / 20);
  char iconb[128] ="";
  for (const auto& t : tabs) {
    strncat(iconb,t->geticons(),sizeof(iconb)-1);
  }
  boolean is_bluetooth_scanning = true; // todo
  lv_label_set_text_fmt(labelicons, "%s %s %s %s",iconb, is_bluetooth_scanning?LV_SYMBOL_BLUETOOTH:"",batteryicons[level], M5.Power.isCharging()?batteryicons[5]:"");
}

void tab_splashscreen::loop(boolean activetab) {
  if (activetab) {
    for (int i = 0; i < 4; i++) {
      buttonhue[i]= ((millis()%20000*360)/20000+90*i)%360;  // cycle colours every 20s
      m5io_showanalogrgb(i + 1, lv_color_hsv_to_rgb(buttonhue[i], 100, 50));  // rotary LED
    }
    m5io_showanalogrgb(5, lv_color_hsv_to_rgb(0, 0, 5));  // cherry LED (very bright)
  }
  if (batterycheckmillis == 0 || (millis() - batterycheckmillis) > 20000) { // every 20 sec
    updateicons();
    batterycheckmillis = millis();
  }
}

#include "tab-mqtt.hpp"

void tab_splashscreen::switch_change(int sw, boolean value) {
  ESP_LOGI("splashscreen", "new callback button %d %s", sw, value ? "push" : "release");
  if (sw == 4 && value) {
    dump_connected_devices();
    updateicons();
  }
  if (sw == 1 && value) {
    for (const auto& st: tabs) {
      if (!strncmp(st->gettabname(),"wifi",4)) {
        tab_mqtt *t = static_cast<tab_mqtt *>(st);
        t->popup_add_device(page);
      }
    }
  }
}

void tab_splashscreen::encoder_change(int sw, int change) {
  ESP_LOGI("splashscreen", "Encoder %d: %+d", sw, change);
}

void tab_splashscreen::setup(void) {
  page = lv_tabview_add_tab(tv, gettabname());

  lv_obj_set_style_pad_left(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_right(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(page, 0, LV_PART_MAIN);

  // always first child
  lv_obj_t *lv_debug_window = lv_textarea_create(page);
  lv_textarea_add_text(lv_debug_window, "");
  lv_textarea_set_cursor_click_pos(lv_debug_window, false);
  lv_obj_set_size(lv_debug_window, lv_pct(100), lv_pct(60));
  lv_obj_set_align(lv_debug_window, LV_ALIGN_BOTTOM_LEFT);

  labelicons = lv_label_create(page);
  lv_label_set_text(labelicons, "");
  lv_obj_set_style_text_align(labelicons, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(labelicons, LV_ALIGN_TOP_RIGHT, -8, 0);
  lv_obj_set_style_text_font(labelicons, &lv_font_montserrat_24, LV_PART_MAIN);
}