#include <memory>

#include "device-lovense.hpp"
#include "tab-lovense.hpp"
#include "../../ui/tab.hpp"
#include "../../core/lvgl-utils.h"

tab_lovense::tab_lovense() {
  ison = false;
  lockpanel = false;
  timer = new tab_object_timer(false);
  rand_timer = new tab_object_timer(true);
  sync = new tab_object_sync();
  modeselect = new tab_object_modes();
  modetimer = new tab_object_modetimer(timer, rand_timer, [this](bool active_on) {
      device_lovense *md = static_cast<device_lovense *>(device);
      ison = active_on;
      if (active_on) {
          md->setmodespeed(main_pattern, knob_speed);
          send_sync_data(SYNC_ON);
      } else {
          md->setmodespeed(main_pattern, 0);
          send_sync_data(SYNC_OFF);
      }
  });
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
  knob_speed = 10; // 50%
  main_pattern = 0; // continuous
  battery_time = millis()-50000;
}
tab_lovense::~tab_lovense() {}

void tab_lovense::encoder_change(int sw, int change) {
  device_lovense *md = static_cast<device_lovense *>(device);

  if (sw == tab_object_buttonbar::rotary4) {
    modeselect->rotary_change(change);
    rand_timer->rotary_change(change);
    timer->rotary_change(change);
  }
  if (sw == tab_object_buttonbar::rotary1 && main_pattern == 0) {
    knob_speed = std::min(20,std::max(1,knob_speed+change));
    if (ison) md->setmodespeed(main_pattern,knob_speed);
  }
  if (sw == tab_object_buttonbar::rotary3) {
    main_pattern+=change;
    if (main_pattern<0) main_pattern=md->patterns_n-1;
    if (main_pattern>=md->patterns_n) main_pattern=0;
    if (ison) md->setmodespeed(main_pattern,knob_speed);
    need_refresh = true;
  }

  need_knob_refresh = true;
}

void tab_lovense::switch_change(int sw, bool value) {
  need_refresh = true;
  device_lovense *md = static_cast<device_lovense *>(device);

  if (sw == tab_object_buttonbar::rotary4 && value) {
    if (modeselect->has_focus()) {
      if (!modeselect->highlight_next_field()) {  // false then we left focus                          
       if (main_mode == MODE_RANDOM)
          rand_timer->highlight_next_field();
        else if (main_mode == MODE_TIMER)
          timer->highlight_next_field();
      }
    } else if (rand_timer->has_focus()) {
      rand_timer->highlight_next_field();
    } else if (timer->has_focus()) {
      timer->highlight_next_field();
    } else {
      modeselect->highlight_next_field();
    }
  }

  if (main_mode == MODE_MANUAL) {
    if (sw == tab_object_buttonbar::switch1 && value && ison == 0) {
      ison = true;
      md->setmodespeed(main_pattern,knob_speed);
      send_sync_data(SYNC_ON);
    } else if (sw == tab_object_buttonbar::switch1 && value && ison == 1) {
      ison = false;
      md->setmodespeed(main_pattern,0);
      send_sync_data(SYNC_OFF);
    }
  }
  if (main_mode != MODE_MANUAL && sw == tab_object_buttonbar::switch1 && value) {
    if (ison) {
      ison = false;
      md->setmodespeed(main_pattern,0);
      send_sync_data(SYNC_OFF);
    }
    main_mode = MODE_MANUAL;
    modetimer->set_is_on(false);
    modeselect->reset();
  }
  if (sw == tab_object_buttonbar::rotary3 && value) {
    main_pattern++;
    if (main_pattern>=md->patterns_n) main_pattern=0;
    if (ison) md->setmodespeed(main_pattern,knob_speed);
  }
}

// another device can push data to us when they connect, disconnect, turn on, turn off

void tab_lovense::gotsyncdata(Tab *t, sync_data syncstatus) {
  device_lovense *md = static_cast<device_lovense *>(device);
  if (!md) return;
  ESP_LOGD("lovense", "got sync data %d from %s", syncstatus, t->gettabname());
  if (syncstatus == SYNC_ALLOFF) {
    main_mode = MODE_MANUAL;
    modeselect->reset();
    ison = false;
    md->setmodespeed(main_pattern,0);
    need_refresh = true;
  }
  if (main_mode == MODE_SYNC) {
    bool isinverted = sync->isinverted();

  if ((syncstatus == SYNC_ON && !isinverted) || (syncstatus == SYNC_OFF && isinverted)) {
      ison = true;
      md->setmodespeed(main_pattern,knob_speed);
    } else if ((syncstatus == SYNC_OFF && !isinverted) || (syncstatus == SYNC_ON && isinverted)) {
      ison = false;
      md->setmodespeed(main_pattern,0);
    }
    need_refresh = true;
  }
}

void tab_lovense::loop(bool activetab) {
  device_lovense *md = static_cast<device_lovense *>(device);

  if ( millis() - battery_time > 60000) { // just every minute
    battery_pc = md->ble_lovense_getbattery();
    ESP_LOGI("lovense","Battery %d%%",battery_pc);
    battery_time = millis();
    need_refresh = true;
  }

  if (modetimer->update(main_mode == MODE_RANDOM || main_mode == MODE_TIMER, main_mode == MODE_RANDOM)) {
      need_refresh = true;
  }

  if (activetab && need_refresh) {
    ESP_LOGD("lovense", "refresh active tab from %s on %d",
             pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

    device_lovense *md = static_cast<device_lovense *>(device);
    status->set_active(ison);

    if (battery_pc>0)
      lv_label_set_text_fmt(lv_obj_get_child(tab_battery, 0), "battery %d%%", battery_pc);

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = modetimer->get_remaining_seconds();
      status->set_text_fmt("%s\n%s: %d", md->patterns[main_pattern], ison?"On":"Off", seconds);
    } else {
      status->set_text_fmt("%s\n%s", md->patterns[main_pattern], ison?"On":"Off");
    }
    need_refresh = false;
    need_knob_refresh = true;
  }
  if (activetab && need_knob_refresh) {

    need_knob_refresh = false;
 
    if (main_pattern == 0) {
      buttonbar->set_text_fmt(tab_object_buttonbar::rotary1,"Speed\n%d%%",knob_speed*5);
      buttonbar->set_value(tab_object_buttonbar::rotary1,knob_speed*5-3);
      buttonbar->set_rgb(tab_object_buttonbar::rotary1, ison?lv_color_hsv_to_rgb(0, 100, knob_speed*5): lv_color_hsv_to_rgb(0, 0, 0));
    } else {
      buttonbar->set_text_fmt(tab_object_buttonbar::rotary1,"");
      buttonbar->set_value(tab_object_buttonbar::rotary1,0);
      buttonbar->set_rgb(tab_object_buttonbar::rotary1, ison? lv_color_hsv_to_rgb(0, 100, 100): lv_color_hsv_to_rgb(0, 0, 0));
    }
    if (main_mode == MODE_MANUAL) {
      buttonbar->set_text(tab_object_buttonbar::switch1,"On\nOff");
      buttonbar->set_value(tab_object_buttonbar::switch1,ison? 100:0);
    } else {
      buttonbar->set_text(tab_object_buttonbar::switch1,"Stop");
      buttonbar->set_value(tab_object_buttonbar::switch1,0);
    }
    buttonbar->set_text(tab_object_buttonbar::rotary3,"mode");

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      if (rand_timer->has_focus() || timer->has_focus())
        buttonbar->set_value(tab_object_buttonbar::rotary4,100);
      else
        buttonbar->set_value(tab_object_buttonbar::rotary4,0);
    }
  }
}


void tab_lovense::focus_change(bool focus) {
  ESP_LOGD("lovense", "focus cb %s on %d: %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), focus);
  need_refresh = true;
  buttonbar->set_rgb_all(lv_color_hsv_to_rgb(0, 0, 0));
  buttonbar->set_text(tab_object_buttonbar::rotary4, LV_SYMBOL_SETTINGS);
}


void tab_lovense::tab_create_battery(lv_obj_t *tv2) {
  tab_battery = lv_obj_create(tv2);
  lv_obj_set_size(tab_battery, LV_SCALE(150), LV_SCALE(24));
  lv_obj_align(tab_battery, LV_ALIGN_TOP_LEFT, LV_SCALE(4), LV_SCALE(64+12));
  lv_obj_t *labelx = lv_label_create(tab_battery);
  lv_label_set_text(labelx, "");
  lv_obj_set_style_text_font(labelx, LV_FONT_GET(14), LV_PART_MAIN);
  lv_obj_set_style_text_align(labelx, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_pad_ver(tab_battery, LV_SCALE(3), LV_PART_MAIN);
  lv_obj_align(labelx, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_scrollbar_mode(tab_battery, LV_SCROLLBAR_MODE_OFF);
}


void tab_lovense::tab_create() {
  create_standard_page(lovense_main_modes_c);
  create_standard_widgets();

  tab_create_battery(page);
}

// return false if we removed ourselves from the connected devices list
bool tab_lovense::hardware_changed(void) {
  need_refresh = true;
  if (last_change == D_CONNECTING) {
    printf_log("Connecting %s\n", device->getShortName());
  } else if (last_change == D_CONNECTED) {
    tab_create();
    send_sync_data(SYNC_START);
    send_sync_data(SYNC_OFF);
    ison = false;
    printf_log("Connected %s\n", device->getShortName());
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", device->getShortName());
    send_sync_data(SYNC_BYE);
    return false;
  }
  return true;
}