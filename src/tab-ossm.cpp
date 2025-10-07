#include <memory>

#include "device-ossm.hpp"
#include "tab-ossm.hpp"
#include "tab.hpp"
#include "lvgl-utils.h"

tab_ossm::tab_ossm() {
  ison = false;
  lockpanel = false;
  main_mode = MODE_MANUAL;
  timer = new tab_object_timer(false);
  rand_timer = new tab_object_timer(true);
  sync = new tab_object_sync();
  modeselect = new tab_object_modes();
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
  knob_speed = 0; // 50%
  main_pattern = 0; // continuous
}
tab_ossm::~tab_ossm() {}

void tab_ossm::encoder_change(int sw, int change) {
  device_ossm *md = static_cast<device_ossm *>(device);

  if (sw == tab_object_buttonbar::rotary4) {
    modeselect->rotary_change(change);
    rand_timer->rotary_change(change);
    timer->rotary_change(change);
  }
  if (sw == tab_object_buttonbar::rotary1 && main_pattern == 0) {
    knob_speed = min(100,max(0,knob_speed+change));
    if (ison) md->set_speed(knob_speed);
  }
  if (sw == tab_object_buttonbar::rotary3) {
    main_pattern+=change;
    if (main_pattern<0) main_pattern=md->patterns_n-1;
    if (main_pattern>=md->patterns_n) main_pattern=0;
    //if (ison) md->setmodespeed(main_pattern,knob_speed);
    need_refresh = true;
  }

  need_knob_refresh = true;
}

void tab_ossm::switch_change(int sw, boolean value) {
  need_refresh = true;
  device_ossm *md = static_cast<device_ossm *>(device);

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
      //md->setmodespeed(main_pattern,knob_speed);
      send_sync_data(SYNC_ON);
    } else if (sw == tab_object_buttonbar::switch1 && value && ison == 1) {
      ison = false;
      //md->setmodespeed(main_pattern,0);
      send_sync_data(SYNC_OFF);
    }
  }
  if (main_mode != MODE_MANUAL && sw == tab_object_buttonbar::switch1 && value) {
    if (ison) {
      ison = false;
      //md->setmodespeed(main_pattern,0);
      send_sync_data(SYNC_OFF);
    }
    main_mode = MODE_MANUAL;
    modeselect->reset();
  }
  if (sw == tab_object_buttonbar::rotary3 && value) {
    main_pattern++;
    if (main_pattern>=md->patterns_n) main_pattern=0;
    //if (ison) md->setmodespeed(main_pattern,knob_speed);
  }
}

// another device can push data to us when they connect, disconnect, turn on, turn off

void tab_ossm::gotsyncdata(Tab *t, sync_data syncstatus) {
  device_ossm *md = static_cast<device_ossm *>(device);
  if (!md) return;
  ESP_LOGD("ossm", "got sync data %d from %s", syncstatus, t->gettabname());
  if (syncstatus == SYNC_ALLOFF) {
    main_mode = MODE_MANUAL;
    modeselect->reset();
    ison = false;
    //md->setmodespeed(main_pattern,0);
    need_refresh = true;
  }
  if (main_mode == MODE_SYNC) {
    bool isinverted = sync->isinverted();

  if ((syncstatus == SYNC_ON && !isinverted) || (syncstatus == SYNC_OFF && isinverted)) {
      ison = true;
      //md->setmodespeed(main_pattern,knob_speed);
    } else if ((syncstatus == SYNC_OFF && !isinverted) || (syncstatus == SYNC_ON && isinverted)) {
      ison = false;
      //md->setmodespeed(main_pattern,0);
    }
    need_refresh = true;
  }
}

void tab_ossm::loop(boolean activetab) {
  device_ossm *md = static_cast<device_ossm *>(device);

  if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
    if (timermillis < millis()) {
      need_refresh = true;
      if (!ison) {
        ison = true;
        //md->setmodespeed(main_pattern,knob_speed);
        send_sync_data(SYNC_ON);
        if (main_mode == MODE_RANDOM)
          timermillis = millis() + rand_timer->gettimeon() * 1000;
        else
          timermillis = millis() + timer->gettimeon() * 1000;
      } else {
        ison = false;
        //md->setmodespeed(main_pattern,0);
        send_sync_data(SYNC_OFF);
        if (main_mode == MODE_RANDOM)
          timermillis = millis() + rand_timer->gettimeoff() * 1000;
        else
          timermillis = millis() + timer->gettimeoff() * 1000;
      }
    }
    if (activetab) {
      int seconds = (timermillis - millis()) / 1000;
      static int last_seconds;
      if (seconds != last_seconds) {
        need_refresh = true;
        last_seconds = seconds;
      }
    }
  }

  if (activetab && need_refresh) {
    ESP_LOGD("ossm", "refresh active tab from %s on %d",
             pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

    device_ossm *md = static_cast<device_ossm *>(device);
    lv_obj_set_style_bg_color(tab_status, lv_color_hex(ison?COLOUR_GREEN:COLOUR_RED), LV_PART_MAIN);

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = (timermillis - millis()) / 1000;
      lv_label_set_text_fmt(lv_obj_get_child(tab_status, 0), "%s\n%s: %d", md->patterns[main_pattern], ison?"On":"Off", seconds);
    } else {
      lv_label_set_text_fmt(lv_obj_get_child(tab_status, 0), "%s\n%s", md->patterns[main_pattern], ison?"On":"Off");
    }
    need_refresh = false;
    need_knob_refresh = true;
  }
  if (activetab && need_knob_refresh) {
    device_ossm *md = static_cast<device_ossm *>(device);

    need_knob_refresh = false;
 
    buttonbar->set_text_fmt(tab_object_buttonbar::rotary1,"Speed\n%d%%",knob_speed);
    buttonbar->set_value(tab_object_buttonbar::rotary1,knob_speed);
    buttonbar->set_rgb(tab_object_buttonbar::rotary1, ison?lv_color_hsv_to_rgb(0, 100, knob_speed): lv_color_hsv_to_rgb(0, 0, 0));

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

void ossm_mode_change_cb(lv_event_t *event) {
  tab_ossm *ossm_tab = static_cast<tab_ossm *>(lv_event_get_user_data(event));
  ossm_tab->main_mode = static_cast<tab_ossm::main_modes>(lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(event)));
  ESP_LOGI("ossm", "cb %s on %d: new mode %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), ossm_tab->main_mode);
  ossm_tab->need_refresh = true;
  ossm_tab->rand_timer->show((ossm_tab->main_mode == tab_ossm::MODE_RANDOM));
  ossm_tab->timer->show((ossm_tab->main_mode == tab_ossm::MODE_TIMER));
  ossm_tab->sync->show((ossm_tab->main_mode == tab_ossm::MODE_SYNC));
}

void tab_ossm::focus_change(boolean focus) {
  ESP_LOGD("ossm", "focus cb %s on %d: %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), focus);
  need_refresh = true;
  buttonbar->set_rgb_all(lv_color_hsv_to_rgb(0, 0, 0));
  buttonbar->set_text(tab_object_buttonbar::rotary4, LV_SYMBOL_SETTINGS);
}

void tab_ossm::tab_create_status(lv_obj_t *tv2) {
  tab_status = lv_obj_create(tv2);
  lv_obj_set_size(tab_status, 150, 64);
  lv_obj_align(tab_status, LV_ALIGN_TOP_LEFT, 4, 0);
  lv_obj_set_style_bg_color(tab_status, lv_color_hex(0xFF0000), LV_PART_MAIN);
  lv_obj_t *labelx = lv_label_create(tab_status);
  lv_label_set_text(labelx, "-");
  lv_obj_set_style_text_font(labelx, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_obj_set_style_text_align(labelx, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_pad_top(tab_status, 3, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(tab_status, 3, LV_PART_MAIN);
  lv_obj_align(labelx, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_t *extra_label = lv_label_create(tab_status);
  lv_obj_set_style_text_font(extra_label, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_label_set_text(extra_label, "");
  lv_obj_set_style_text_align(extra_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(extra_label, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_scrollbar_mode(tab_status, LV_SCROLLBAR_MODE_OFF);
}

void tab_ossm::tab_create() {
  page = lv_tabview_add_tab(tv, gettabname());

  lv_obj_set_style_pad_left(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(page, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_right(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(page, 0, LV_PART_MAIN);

  modeselect->createdropdown(page, ossm_main_modes_c);
  lv_obj_add_event_cb(modeselect->getdropdownobject(), ossm_mode_change_cb, LV_EVENT_VALUE_CHANGED, this);

  buttonbar = new tab_object_buttonbar(page);
  tab_create_status(page);
  rand_timer->view(page);
  timer->view(page);
  sync->view(page);

  lv_tabview_set_act(tv, lv_get_tabview_idx_from_page(tv, page), LV_ANIM_OFF);
}

// return false if we removed ourselves from the connected devices list
boolean tab_ossm::hardware_changed(void) {
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