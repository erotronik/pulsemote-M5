#include "device-zc95.hpp"
#include "tab-zc95.hpp"
#include "../../ui/tab.hpp"
#include "../../core/lvgl-utils.h"

tab_zc95::tab_zc95() {
  ison = true; // default power on mode is on, with the levels at 0
  timer = new tab_object_timer(false);
  rand_timer = new tab_object_timer(true);
  sync = new tab_object_sync();
  modeselect = new tab_object_modes();
  patternselect = new tab_object_patterns();
  modetimer = new tab_object_modetimer(timer, rand_timer, [this](bool active_on) {
      ison = active_on;
      if (active_on) {
          zc95_on();
          send_sync_data(SYNC_ON);
      } else {
          zc95_off();
          send_sync_data(SYNC_OFF);
      }
  });
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
}

tab_zc95::~tab_zc95() {}

// Start the wanted pattern (stopping any other first), at the current levels
void tab_zc95::zc95_on(void) {
  device_zc95 *zd = static_cast<device_zc95 *>(device);
  if (!zd->pattern_start(wanted_pattern)) {
    ESP_LOGE("zc95", "couldn't start pattern %d", wanted_pattern);
    ison = false;
    need_refresh = true;
    return;
  }
  send_levels();
}

void tab_zc95::zc95_off(void) {
  device_zc95 *zd = static_cast<device_zc95 *>(device);
  zd->pattern_stop();
}

void tab_zc95::send_levels(void) {
  device_zc95 *zd = static_cast<device_zc95 *>(device);
  zd->set_power(level[0] * 10, level[1] * 10, level[2] * 10, level[3] * 10);
}

// The highest level worth setting for a channel: the box's front panel knob caps it anyway
int tab_zc95::max_level(int channel) {
  device_zc95 *zd = static_cast<device_zc95 *>(device);
  return std::min(99, zd->get_power_limit(channel) / 10);
}

void tab_zc95::change_level(int channel, int change) {
  level[channel] = std::min(max_level(channel), std::max(0, level[channel] + change));
  send_levels();
  need_knob_refresh = true;
}

// rotary4 has been clicked into the mode/timer settings
bool tab_zc95::settings_have_focus(void) {
  return modeselect->has_focus() || timer->has_focus() || rand_timer->has_focus();
}

// Each rotary sets a channel's level. Once clicked, rotary3 scrolls the pattern list and rotary4
// changes the settings, until they're clicked out again
void tab_zc95::encoder_change(int sw, int change) {
  if (sw == tab_object_buttonbar::rotary1) {
    change_level(0, change);
  }
  if (sw == tab_object_buttonbar::rotary2) {
    change_level(1, change);
  }
  if (sw == tab_object_buttonbar::rotary3) {
    if (patternselect->visible()) {
      patternselect->rotary_change(change);
      need_refresh = true;
    } else {
      change_level(2, change);
    }
  }
  if (sw == tab_object_buttonbar::rotary4) {
    if (settings_have_focus()) {
      rand_timer->rotary_change(change);
      timer->rotary_change(change);
      modeselect->rotary_change(change);
    } else {
      change_level(3, change);
    }
  }
}

void tab_zc95::switch_change(int sw, bool value) {
  need_refresh = true;

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
    if (!ison) {
      ison = true;
      zc95_on();
      send_sync_data(SYNC_ON);
    } else {
      ison = false;
      zc95_off();
      send_sync_data(SYNC_OFF);
    }
  }

  if (main_mode != MODE_MANUAL && sw == tab_object_buttonbar::switch1 && value) {  // Stop
    main_mode = MODE_MANUAL;
    modetimer->set_is_on(false);
    modeselect->reset();
    ison = false;
    zc95_off();
  }

  if (sw == tab_object_buttonbar::rotary3 && value) {
    if (!patternselect->visible()) {
      patternselect->show(wanted_pattern);
    } else {
      wanted_pattern = patternselect->hide();
    }
  }
}

// another device can push data to us when they connect, disconnect, turn on, turn off

void tab_zc95::gotsyncdata(Tab *t, sync_data syncstatus) {
  if (!device) return;
  ESP_LOGD("zc95", "got sync data %d from %s", syncstatus, t->gettabname());
  if (syncstatus == SYNC_BUTTONPRESS) {
    for (int i = 0; i < channels; i++) {
      last_level_button_press[i] = level[i];
      level[i] = std::min(max_level(i), std::max(0, level[i] + 5+rand()%(11-5)));
    }
    send_levels();
    need_knob_refresh = true;
  }
  if (syncstatus == SYNC_BUTTONRELEASE) {
    for (int i = 0; i < channels; i++) level[i] = last_level_button_press[i];
    send_levels();
    need_knob_refresh = true;
  }
  if (syncstatus == SYNC_ALLOFF) {
    main_mode = MODE_MANUAL;
    modeselect->reset();
    ison = false;
    zc95_off();
    need_refresh = true;
  }
  if (main_mode == MODE_SYNC) {
    bool isinverted = sync->isinverted();
    if ((syncstatus == SYNC_ON && !isinverted) || (syncstatus == SYNC_OFF && isinverted)) {
      ison = true;
      zc95_on();
    } else if ((syncstatus == SYNC_OFF && !isinverted) || (syncstatus == SYNC_ON && isinverted) ||
               syncstatus == SYNC_BYE) {  // the source has gone away
      ison = false;
      zc95_off();
    }
    need_refresh = true;
  }
}

void tab_zc95::loop(bool activetab) {
  device_zc95 *zd = static_cast<device_zc95 *>(device);
  if (!zd) return;
  zd->poll();

  // Bring levels down if a front panel knob on the box has been turned below them
  bool capped = false;
  for (int i = 0; i < channels; i++) {
    if (level[i] > max_level(i)) {
      level[i] = max_level(i);
      capped = true;
    }
  }
  if (capped) {
    send_levels();
    need_knob_refresh = true;
  }

  if (modetimer->update(main_mode == MODE_RANDOM || main_mode == MODE_TIMER, main_mode == MODE_RANDOM)) {
      need_refresh = true;
  }

  // A different pattern was picked while running: switch to it
  if (ison && zd->connected() && zd->get_running_pattern() != -1 && wanted_pattern != zd->get_running_pattern()) {
    ESP_LOGD("zc95", "need to change pattern to %d currently %d", wanted_pattern, zd->get_running_pattern());
    zc95_on();
    need_refresh = true;
  }

  if (activetab) set_battery_label(zd->get_battery());

  if (activetab && need_refresh) {
    status->set_active(ison);
    const char *name = zd->get_pattern_name(wanted_pattern);
    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = modetimer->get_remaining_seconds();
      status->set_text_fmt("%s\n%d", name, seconds);
    } else {
      status->set_text(name);
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

    const int rotary[channels] = { tab_object_buttonbar::rotary1, tab_object_buttonbar::rotary2,
                                   tab_object_buttonbar::rotary3, tab_object_buttonbar::rotary4 };
    for (int i = 0; i < channels; i++) {
      char name[2] = { (char)('1' + i), '\0' };
      buttonbar->set_level(rotary[i], name, level[i]);
      // rotary3/4 show what they're doing instead of their level when clicked into the pattern list or
      // settings, though it's still their level
      if (i == 2 && patternselect->visible()) {
        buttonbar->set_text(rotary[i], "mode");
        buttonbar->set_value(rotary[i], 100);
        if (activetab) buttonbar->set_rgb(rotary[i], lv_color_hsv_to_rgb(0, 0, 0));
        continue;
      }
      if (i == 3 && settings_have_focus()) {
        buttonbar->set_text(rotary[i], LV_SYMBOL_SETTINGS);
        buttonbar->set_value(rotary[i], 100);
        if (activetab) buttonbar->set_rgb(rotary[i], lv_color_hsv_to_rgb(0, 0, 0));
        continue;
      }
      buttonbar->set_ison(rotary[i], ison);
      if (activetab) buttonbar->set_rgb(rotary[i], lv_color_hsv_to_rgb(0, 100, level[i]));
    }
  }
}

void zc95_pattern_change_cb(lv_event_t *event) {
  tab_zc95 *zc95_tab = static_cast<tab_zc95 *>(lv_event_get_user_data(event));
  lv_obj_t *dropdown = (lv_obj_t *)lv_event_get_target(event);

  // Turning rotary3 changes the selection while the list is open: that's just scrolling. The pattern is
  // picked when the list closes, by clicking rotary3 (see switch_change()) or touching an option.
  if (lv_dropdown_is_open(dropdown)) return;

  int i = lv_dropdown_get_selected(dropdown);
  ESP_LOGI("zc95", "new pattern %d", i);
  zc95_tab->wanted_pattern = i;
  zc95_tab->need_refresh = true;
  zc95_tab->patternselect->hide();
}

void tab_zc95::focus_change(bool focus) {
  need_refresh = true;
  buttonbar->set_rgb_all(lv_color_hsv_to_rgb(0, 0, 0));
}

void tab_zc95::tab_create() {
  device_zc95 *zd = static_cast<device_zc95 *>(device);

  create_standard_page(zc95_main_modes_c);
  create_standard_widgets();

  buttonbar->set_onmain(tab_object_buttonbar::rotary1, true);
  buttonbar->set_onmain(tab_object_buttonbar::rotary2, true);
  buttonbar->set_onmain(tab_object_buttonbar::rotary3, true);
  buttonbar->set_onmain(tab_object_buttonbar::rotary4, true);
  buttonbar->set_click_text(tab_object_buttonbar::rotary1, "");
  buttonbar->set_click_text(tab_object_buttonbar::rotary2, "");
  buttonbar->set_click_text(tab_object_buttonbar::rotary3, "mode");
  buttonbar->set_click_text(tab_object_buttonbar::rotary4, LV_SYMBOL_SETTINGS);

  create_battery_label(page);

  // Patterns are read from the box when it connects
  patternselect->selectpattern(page, zd->get_pattern_names(), zd->pattern_count());
  lv_obj_add_event_cb(patternselect->getdropdownobject(), zc95_pattern_change_cb, LV_EVENT_VALUE_CHANGED, this);
}

// return false if we removed ourselves from the connected devices list
bool tab_zc95::hardware_changed(void) {
  need_refresh = true;
  if (last_change == D_CONNECTING) {
    printf_log("Connecting %s\n", device->getShortName());
  } else if (last_change == D_CONNECTED) {
    tab_create();
    send_sync_data(SYNC_START);
    send_sync_data(SYNC_ON);
    zc95_on();
    printf_log("Connected %s\n", device->getShortName());
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", device->getShortName());
    send_sync_data(SYNC_BYE);
    return false;
  }
  return true;
}
