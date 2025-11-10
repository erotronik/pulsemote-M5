#include "M5Unified.h"
#include <memory>

#include "tab-object-timer.hpp"
#include "tab-splashscreen.hpp"
#include "tab.hpp"
#include "lvgl-utils.h"
#include "comms-wifi.hpp"
#include "tab-mqtt.hpp"

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
  int level = max(0,min(4,M5.Power.getBatteryLevel() / 20));
  char iconb[128] ="";
  for (const auto& t : tabs) {
    strncat(iconb,t->geticons(),sizeof(iconb)-1);
  }
  boolean is_bluetooth_scanning = true; // todo
  lv_label_set_text_fmt(labelicons, "%s %s %s %s",iconb, is_bluetooth_scanning?LV_SYMBOL_BLUETOOTH:"",batteryicons[level], M5.Power.isCharging()?batteryicons[5]:"");
}

void tab_splashscreen::loop(boolean activetab) {
  if (activetab) {
    const byte map[]={3,2,0,1};
    for (int i = 0; i < 4; i++) {
      buttonhue[map[i]]= ((millis()%20000*360)/20000+20*i)%360;  // cycle colours every 20s
      m5io_showanalogrgb(map[i] + 1, lv_color_hsv_to_rgb(buttonhue[map[i]], 100, 50));  // rotary LED
    }
    m5io_showanalogrgb(5, lv_color_hsv_to_rgb(0, 0, 5));  // cherry LED (very bright)
  }
  if (batterycheckmillis == 0 || (millis() - batterycheckmillis) > 2000) { // every 2 sec
    updateicons();
    batterycheckmillis = millis();
  }
}

void tab_splashscreen::popup_add_wifi_device() {
  for (const auto& st: tabs) {
    if (!strncmp(st->gettabname(),"wifi",4)) {
      tab_mqtt *t = static_cast<tab_mqtt *>(st);
      t->popup_add_device(tabs.front()->page);
    }
  }
}

void tab_splashscreen::switch_change(int sw, boolean value) {
  ESP_LOGI("splashscreen", "new callback button %d %s", sw, value ? "push" : "release");
  if (sw == tab_object_buttonbar::switch1 && value) {
    popup_add_wifi_device();
  }
  if (sw == tab_object_buttonbar::rotary1 && value) {
    dump_connected_devices();
    updateicons();
  }
}

void tab_splashscreen::encoder_change(int sw, int change) {
  ESP_LOGI("splashscreen", "Encoder %d: %+d", sw, change);
}

void tab_splashscreen::setup(void) {
  page = lv_tabview_add_tab(tv, gettabname());

// 2 columns (left/right), 3 rows (header / flexible middle / footer)
static int32_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
static int32_t rows[] = { LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST };
lv_obj_set_grid_dsc_array(page, cols, rows);

lv_obj_set_style_pad_all(page, 0, 0);
lv_obj_set_style_pad_column(page, 0, 0);
lv_obj_set_style_pad_row(page, 4, 0);
lv_obj_set_style_pad_top(page, 4, 0);

labelicons = lv_label_create(page);
lv_label_set_text(labelicons, "");
lv_obj_set_style_text_align(labelicons, LV_TEXT_ALIGN_RIGHT, 0);
lv_obj_set_style_text_font(labelicons, &lv_font_montserrat_24, LV_PART_MAIN);
lv_obj_set_grid_cell(labelicons, LV_GRID_ALIGN_END, 1, 1, LV_GRID_ALIGN_START, 0, 1);

lv_debug_window = lv_textarea_create(page);
lv_textarea_add_text(lv_debug_window, "");
lv_textarea_set_cursor_click_pos(lv_debug_window, false);
lv_obj_set_grid_cell(lv_debug_window, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_STRETCH, 1, 1);
lv_obj_set_width(lv_debug_window, lv_pct(100));
lv_obj_set_style_text_font(lv_debug_window, &lv_font_montserrat_12, LV_PART_MAIN);

buttonbar = new tab_object_buttonbar(page);
buttonbar->set_text(tab_object_buttonbar::switch1,"Add\nDevice");

lv_obj_set_style_pad_all(buttonbar->container, 0, 0);
lv_obj_set_style_margin_all(buttonbar->container, 0, 0);
lv_obj_set_grid_cell(buttonbar->container, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_END, 2, 1);

}