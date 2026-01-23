#include "device-mk312.hpp"
#include "tab-mk312.hpp"
#include "tab.hpp"
#include "lvgl-utils.h"

tab_mk312::tab_mk312() {
  ison = true; // default power on mode is on
  lockpanel = false; // lockpanel means we lock out the front A/B level knobs and control the levels from software
  main_mode = MODE_MANUAL;
  timer = new tab_object_timer(false);
  rand_timer = new tab_object_timer(true);
  sync = new tab_object_sync();
  modeselect = new tab_object_modes();
  patternselect = new tab_object_patterns();
  status = nullptr;
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
}

tab_mk312::~tab_mk312() {}

void tab_mk312::encoder_change(int sw, int change) {
  device_mk312 *md = static_cast<device_mk312 *>(device);
  if (sw == tab_object_buttonbar::rotary2 && lockpanel) {
    level_b = std::min(99, std::max(0, level_b + change));
    md->etbox_setlevelb(level_b);
    need_knob_refresh = true;
  }
  if (sw == tab_object_buttonbar::rotary1 && lockpanel) {
    level_a = std::min(99, std::max(0, level_a + change));
    md->etbox_setlevela(level_a);
    need_knob_refresh = true;
  }
  if (sw == tab_object_buttonbar::rotary4) {
    rand_timer->rotary_change(change);
    timer->rotary_change(change);
    modeselect->rotary_change(change);
  }
  if (sw == tab_object_buttonbar::rotary3) {
    if (!patternselect->visible()) {
      ESP_LOGE("mk312","rotary3 show patternselect %d", wanted_mode);
      patternselect->show(wanted_mode);
    }
    patternselect->rotary_change(change);
    need_refresh = true;
  }
}

void tab_mk312::switch_change(int sw, bool value) {
  need_refresh = true;
  device_mk312 *md = static_cast<device_mk312 *>(device);

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

  if (main_mode == MODE_MANUAL && sw == tab_object_buttonbar::switch1 && value) {
    if (ison == 0) {
      ison = 1;
      md->etbox_on(wanted_mode);
      send_sync_data(SYNC_ON);
    } else {
      ison = 0;
      md->etbox_off();
      send_sync_data(SYNC_OFF);
    }
  }

  if (main_mode != MODE_MANUAL && sw == tab_object_buttonbar::switch1 && value) {  // Stop
    device_mk312 *md = static_cast<device_mk312*>(device);   
    main_mode = MODE_MANUAL;
    modeselect->reset();
    ison = false;
    md->etbox_off();
  }
  if ((sw == tab_object_buttonbar::rotary1 || sw == tab_object_buttonbar::rotary2) && value) {
    lockpanel = !lockpanel;
    md->etbox_setpanellock(lockpanel);
    buttonbar->set_onmain(tab_object_buttonbar::rotary1, lockpanel);
    buttonbar->set_onmain(tab_object_buttonbar::rotary2, lockpanel);
    if (lockpanel) {
      level_a = 0;
      level_b = 0;
      md->etbox_setlevelb(level_b);
      md->etbox_setlevela(level_a);
    }
  }
  if (sw == tab_object_buttonbar::rotary3 && value) {
    if (!patternselect->visible()) {
      ESP_LOGE("mk312","switch show patternselect %d", wanted_mode);
      patternselect->show(wanted_mode);
    } else {
      wanted_mode = patternselect->hide();
    }
    need_refresh = true;
  }
}

// another device can push data to us when they connect, disconnect, turn on, turn off

void tab_mk312::gotsyncdata(Tab *t, sync_data syncstatus) {
  device_mk312 *md = static_cast<device_mk312 *>(device);
  if (!md) return;
  ESP_LOGD("mk312", "got sync data %d from %s", syncstatus, t->gettabname());
  if (syncstatus == SYNC_BUTTONPRESS && lockpanel) {
    last_level_button_press_a = level_a;
    last_level_button_press_b = level_b;
    level_a = std::min(99, std::max(0, level_a + 5+rand()%(11-5)));
    md->etbox_setlevela(level_a);
    level_b = std::min(99, std::max(0, level_b + 5+rand()%(11-5)));
    md->etbox_setlevelb(level_b);
    need_knob_refresh = true;
  }
  if (syncstatus == SYNC_BUTTONRELEASE && lockpanel) {
    level_a = last_level_button_press_a;
    md->etbox_setlevela(level_a);
    level_b = last_level_button_press_b;
    md->etbox_setlevelb(level_b);
    need_knob_refresh = true;
  }
  if (syncstatus == SYNC_ALLOFF) {
    main_mode = MODE_MANUAL;
    modeselect->reset();
    ison = false;
    md->etbox_off();
    need_refresh = true;
  }
  if (main_mode == MODE_SYNC) {
    bool isinverted = sync->isinverted();
    if ((syncstatus == SYNC_ON && !isinverted) || (syncstatus == SYNC_OFF && isinverted)) {
      ison = true;
      md->etbox_on(wanted_mode);
    } else if ((syncstatus == SYNC_OFF && !isinverted) || (syncstatus == SYNC_ON && isinverted)) {
      ison = false;
      md->etbox_off();
    }
    need_refresh = true;
  }
}

void tab_mk312::loop(bool activetab) {
  device_mk312 *md = static_cast<device_mk312 *>(device);
  if (!md) return;

  if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
    if (timermillis < millis()) {
      need_refresh = true;
      if (ison == 0) {
        ison = 1;
        md->etbox_on(wanted_mode);
        send_sync_data(SYNC_ON);
        if (main_mode == MODE_RANDOM)
          timermillis = millis() + rand_timer->gettimeon() * 1000;
        else
          timermillis = millis() + timer->gettimeon() * 1000;
      } else {
        ison = 0;
        md->etbox_off();
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

  if (ison && md->connected() && wanted_mode != -1 && wanted_mode != md->get_last_mode()) {
    ESP_LOGD("et312","need to set mode to %d currently %d", wanted_mode, md->get_last_mode());
    md->set_mode(wanted_mode);
  }

  if (activetab && need_refresh) {
    ESP_LOGD("mk312", "refresh active tab from %s on %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

    status->set_active(ison);
    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = (timermillis - millis()) / 1000;
      status->set_text_fmt("%s\n%d", md->etmodes[(ison||wanted_mode==-1)?md->get_last_mode():wanted_mode], seconds);
    } else {
      status->set_text(md->etmodes[(ison||wanted_mode==-1)?md->get_last_mode():wanted_mode]);
    }
    need_refresh = false;
    need_knob_refresh = true;
  }
  if (need_knob_refresh) {

    need_knob_refresh = false;
    if (main_mode == MODE_MANUAL) {
      buttonbar->set_text(tab_object_buttonbar::switch1,"On\nOff");
      buttonbar->set_value(tab_object_buttonbar::switch1,ison? 100:0);
    } else {
      buttonbar->set_text(tab_object_buttonbar::switch1,"Stop");
      buttonbar->set_value(tab_object_buttonbar::switch1,0);
    }

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      if (rand_timer->has_focus() || timer->has_focus())
        buttonbar->set_value(tab_object_buttonbar::rotary4,100);
      else
        buttonbar->set_value(tab_object_buttonbar::rotary4,0);
    }

    buttonbar->set_text(tab_object_buttonbar::rotary3, "mode");
    if (lockpanel) {
      buttonbar->set_click_text(tab_object_buttonbar::rotary1,"");
      buttonbar->set_value(tab_object_buttonbar::rotary1,level_a);
      buttonbar->set_ison(tab_object_buttonbar::rotary1, ison);
      buttonbar->set_text_fmt(tab_object_buttonbar::rotary1, "A\n%" LV_PRId32 "%%", level_a);
      if (activetab) buttonbar->set_rgb(tab_object_buttonbar::rotary1, lv_color_hsv_to_rgb(0, 100, level_a));
      buttonbar->set_value(tab_object_buttonbar::rotary2,level_b);
      buttonbar->set_ison(tab_object_buttonbar::rotary2, ison);
      buttonbar->set_text_fmt(tab_object_buttonbar::rotary2, "B\n%" LV_PRId32 "%%", level_b);
      if (activetab) buttonbar->set_rgb(tab_object_buttonbar::rotary2, lv_color_hsv_to_rgb(0, 100, level_b));
    } else {
      buttonbar->set_click_text(tab_object_buttonbar::rotary1,"unlock");
      if (activetab) buttonbar->set_rgb(tab_object_buttonbar::rotary1, lv_color_hsv_to_rgb(0, 0, 0));
      if (activetab) buttonbar->set_rgb(tab_object_buttonbar::rotary2, lv_color_hsv_to_rgb(0, 0, 0));
      buttonbar->set_value(tab_object_buttonbar::rotary1,0);
      buttonbar->set_value(tab_object_buttonbar::rotary2,0);
      buttonbar->set_text(tab_object_buttonbar::rotary1, LV_SYMBOL_CHARGE);
      buttonbar->set_text(tab_object_buttonbar::rotary2, LV_SYMBOL_CHARGE);
    }
  }
}

void mk312_mode_change_cb(lv_event_t *event) {
  tab_mk312 *mk312_tab = static_cast<tab_mk312 *>(lv_event_get_user_data(event));
  mk312_tab->main_mode = static_cast<tab_mk312::main_modes>(lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(event)));
  ESP_LOGI("mk312", "cb %s on %d: new mode %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), mk312_tab->main_mode);
  mk312_tab->need_refresh = true;
  mk312_tab->rand_timer->show((mk312_tab->main_mode == tab_mk312::MODE_RANDOM));
  mk312_tab->timer->show((mk312_tab->main_mode == tab_mk312::MODE_TIMER));
  mk312_tab->sync->show((mk312_tab->main_mode == tab_mk312::MODE_SYNC));
}

void mk312_pattern_change_cb(lv_event_t *event) {
  tab_mk312 *mk312_tab = static_cast<tab_mk312 *>(lv_event_get_user_data(event));
  int i = static_cast<tab_mk312::main_modes>(lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(event)));
  ESP_LOGI("mk312", "cb %s on %d: new pattern %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), i);
  mk312_tab->wanted_mode = i;
  mk312_tab->need_refresh = true;
  mk312_tab->patternselect->hide();
}

void tab_mk312::focus_change(bool focus) {
  ESP_LOGD("mk312", "focus cb %s on %d: %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), focus);
  need_refresh = true;
  buttonbar->set_rgb_all(lv_color_hsv_to_rgb(0, 0, 0));
  buttonbar->set_text(tab_object_buttonbar::rotary4, LV_SYMBOL_SETTINGS);
}


void tab_mk312::tab_create() {
  device_mk312 *md = static_cast<device_mk312 *>(device);

  page = lv_tabview_add_tab(tv, gettabname());

  lv_obj_add_style(page, &lvpulsemote_style_tab, LV_PART_MAIN);

  modeselect->createdropdown(page, mk312_main_modes_c);
  lv_obj_add_event_cb(modeselect->getdropdownobject(), mk312_mode_change_cb, LV_EVENT_VALUE_CHANGED, this);

  buttonbar = new tab_object_buttonbar(page);
  buttonbar->set_onmain(tab_object_buttonbar::rotary1, true);
  buttonbar->set_onmain(tab_object_buttonbar::rotary2, true);

  status = new tab_object_status(page, 160-8-8, 64);
  status->align(LV_ALIGN_TOP_LEFT, LV_SCALE(4), 0);

  rand_timer->view(page);
  timer->view(page);
  sync->view(page);

  patternselect->selectpattern(page, md->etmodes , md->etmodes_n);
  lv_obj_add_event_cb(patternselect->getdropdownobject(), mk312_pattern_change_cb, LV_EVENT_VALUE_CHANGED, this);

  lv_tabview_set_act(tv, lv_get_tabview_idx_from_page(tv, page), LV_ANIM_OFF);
}

// return false if we removed ourselves from the connected devices list
bool tab_mk312::hardware_changed(void) {
  ESP_LOGD("mk312","hardware changed %d", last_change);
  need_refresh = true;
  if (last_change == D_CONNECTING) {
    printf_log("Connecting %s\n", device->getShortName());
  } else if (last_change == D_CONNECTED) {
    device_mk312 *md = static_cast<device_mk312 *>(device);
    tab_create();
    send_sync_data(SYNC_START);
    send_sync_data(SYNC_ON);
    md->etbox_on(wanted_mode);
    printf_log("Connected %s\n", device->getShortName());
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", device->getShortName());
    send_sync_data(SYNC_BYE);
    return false;
  }
  return true;
}