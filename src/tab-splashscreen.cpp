#ifdef M5_BOARD
#include "M5Unified.h"
#endif
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

void tab_splashscreen::updateicons() {
  #ifdef M5_BOARD
  int level = max(0,min(4,M5.Power.getBatteryLevel() / 20));
  #else
  int level = 0;
  #endif
  char iconb[128] ="";
  for (const auto& t : tabs) {
    strncat(iconb,t->geticons(),sizeof(iconb)-1);
  }
  boolean is_bluetooth_scanning = true; // todo
  #ifdef M5_BOARD
  bool charging = M5.Power.isCharging();
  lv_label_set_text_fmt(labelicons, "%s %s %s %s",iconb, is_bluetooth_scanning?LV_SYMBOL_BLUETOOTH:"",batteryicons[level], charging?batteryicons[5]:"");
  #else
  lv_label_set_text_fmt(labelicons, "%s %s",iconb, is_bluetooth_scanning?LV_SYMBOL_BLUETOOTH:"");
  #endif
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
  if (activetab && (batterycheckmillis == 0 || (millis() - batterycheckmillis) > 2000)) { // every 2 sec
    updateicons();
    batterycheckmillis = millis();
  }
  if (needs_refresh && activetab) {
    buttonbar->set_text(tab_object_buttonbar::switch1, "Add\nDevice");

    found_count = 0;
 
    // Peek into the button bars of all the tabs, and copy out the ones that are the 'default' controls
    for (auto * st: tabs) {
      auto * bbar = st->buttonbar;
      if (bbar) {
        for (int i=0; i< buttonbar->maxbuttons; i++) {
          int j = buttonbar->rotary_order[i];
          if (j!=-1) {
            char *tx = bbar->get_text(j);
            if (tx && found_count < kMaxFound) {
              found[found_count++] = { st, j, tx, bbar->get_ison(j), bbar->get_value(j) };
              ESP_LOGD("","%s=%s=%d\n",st->gettabname(), tx, bbar->get_value(j));
            }
          }
        }
      }
    }
    for (int i=0; i < kMaxFound; i++) {
      int j = buttonbar->rotary_order[i];
      if (i < found_count) {
        auto st = found[i].tab;
        buttonbar->set_text_fmt(j, "%s\n%s", st->gettabname(), found[i].name);
        buttonbar->set_value(j, found[i].value);
        buttonbar->set_ison(j, found[i].ison);
      } else {
        buttonbar->set_text(j, "");
        buttonbar->set_value(j, 0);
      }
    }
    needs_refresh = false;
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
  for (size_t i = 0; i < found_count; ++i) {
    if (sw == buttonbar->rotary_order[i]) {
      auto* st  = found[i].tab;
      lv_tabview_set_act(tv,lv_get_tabview_idx_from_page(tv,st->page),LV_ANIM_OFF);
    }
  }
  needs_refresh = true;
}


void tab_splashscreen::encoder_change(int sw, int change) {
  ESP_LOGI("splashscreen", "Encoder %d: %+d", sw, change);

  for (size_t i = 0; i < found_count; ++i) {
    if (sw == buttonbar->rotary_order[i]) {
      auto* st  = found[i].tab;
      st->encoder_change(found[i].control, change);
      st->loop(false);
    }
  }
  needs_refresh = true;
}

void tab_splashscreen::focus_change(boolean focus) {
  needs_refresh = true;
}

void tab_splashscreen::gotsyncdata(Tab *t, sync_data syncstatus) {
  needs_refresh = true;
}


void tab_splashscreen::setup(void) {
  page = lv_tabview_add_tab(tv, gettabname());

  lv_obj_add_style(page, &lvpulsemote_style_tab, LV_PART_MAIN);

  lv_obj_set_style_pad_all(page, 0, 0);
  lv_obj_set_style_pad_top(page, 4, 0);

  lv_obj_t *dbg_wrap = lv_obj_create(page);
  lv_obj_set_width(dbg_wrap, LV_PCT(100));
  lv_obj_set_flex_grow(dbg_wrap, 1);
  lv_obj_set_style_pad_all(dbg_wrap, 0, 0);
  lv_obj_set_style_bg_opa(dbg_wrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(dbg_wrap, 0, 0);
  lv_obj_clear_flag(dbg_wrap, LV_OBJ_FLAG_SCROLLABLE);

  lv_debug_window = lv_textarea_create(dbg_wrap);
  lv_textarea_add_text(lv_debug_window, "");
  lv_textarea_set_cursor_click_pos(lv_debug_window, false);
  lv_obj_set_size(lv_debug_window, LV_PCT(100), LV_PCT(100));
  lv_obj_align(lv_debug_window, LV_ALIGN_TOP_MID, 0, 14); // don't align with icons
  lv_obj_set_style_text_font(lv_debug_window, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_clear_flag(lv_debug_window, LV_OBJ_FLAG_CLICK_FOCUSABLE);

  lv_obj_t *icons_bg = lv_obj_create(dbg_wrap);
  lv_obj_remove_style_all(icons_bg);
  lv_obj_set_size(icons_bg, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(icons_bg, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_set_style_bg_opa(icons_bg, LV_OPA_40, 0); 
  lv_obj_set_style_radius(icons_bg, 6, 0);
  lv_obj_set_style_pad_all(icons_bg, 4, 0); 
  lv_obj_align(icons_bg, LV_ALIGN_TOP_RIGHT, -0, 2);
  lv_obj_clear_flag(icons_bg, LV_OBJ_FLAG_CLICKABLE); // no flashing cursor

  labelicons = lv_label_create(icons_bg);
  lv_label_set_text(labelicons, ""); 
  lv_obj_set_style_text_font(labelicons, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(labelicons, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_center(labelicons);
  lv_obj_move_foreground(icons_bg);

  buttonbar = new tab_object_buttonbar(page);
  lv_obj_set_style_pad_all(buttonbar->container, 0, 0);
  lv_obj_set_style_margin_all(buttonbar->container, 0, 0);
  lv_obj_set_width(buttonbar->container, LV_PCT(100));
  needs_refresh = true;

}
