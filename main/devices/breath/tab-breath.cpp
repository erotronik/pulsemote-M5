#include <memory>
#include "device-breath.hpp"
#include "tab-breath.hpp"
#include "../../ui/tab.hpp"
#include "../../core/lvgl-utils.h"
#include <cmath>

tab_breath::tab_breath() {
  timer = new tab_object_timer(false);
  rand_timer = new tab_object_timer(true);
  manual_timer = new tab_object_timer(false, true);
  timer->set_max_on(MAX_RUNTIME_MS / 1000);
  rand_timer->set_max_on(MAX_RUNTIME_MS / 1000);
  manual_timer->set_max_on(MAX_RUNTIME_MS / 1000);
  sync = new tab_object_sync();
  modeselect = new tab_object_modes();
  modetimer = new tab_object_modetimer(timer, rand_timer, [this](bool active_on) {
      if (active_on) {
          set_breath_block(get_breath_pc(current_pattern_mode));
          send_sync_data(SYNC_ON);
      } else {
          set_breath_block(0);
          send_sync_data(SYNC_OFF);
      }
  });
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
  current_pattern_mode = P_RESTRICT;
  breath_block = 0;
}
tab_breath::~tab_breath() {}

void tab_breath::breath_mode_change_cb(lv_event_t *event) {
  tab_breath *tab = static_cast<tab_breath *>(lv_event_get_user_data(event));
  if (!tab) return;

  tab->main_mode = static_cast<main_modes>(lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(event)));
  tab->need_refresh = true;
  tab->need_knob_refresh = true;

  tab->timed_cycle_active = false;
  tab->manual_on_until = 0;
  if (tab->modetimer) tab->modetimer->set_is_on(false);
  if (tab->breath_block != 0) {
    tab->set_breath_block(0);
    tab->send_sync_data(SYNC_OFF);
  }

  tab->update_standard_mode_visibility();
}

void tab_breath::set_breath_block(int pc) {
  if (pc != 0 && last_block_end != 0 && millis() < last_block_end + RECOVERY_MS) {
    ESP_LOGI("breath", "Ignoring set_breath_block(%d) — in recovery period", pc);
    return;
  }
  if (pc < 100 && breath_block == 100) last_block_end = millis();
  breath_block = pc;
  send_breath_block();
  if (breath_block == 100) last_block_start = millis();
}

void tab_breath::send_breath_block() {
  device_breath *md = static_cast<device_breath *>(device);
  ESP_LOGI("breath", "Sending breath block %d", breath_block);
  if (!md) return;
  last_block_sent = millis();
  md->breath_block(breath_block);
}

void tab_breath::encoder_change(int sw, int change) {
  if (sw == tab_object_buttonbar::rotary1 && breath_block == 0) {
    current_pattern_mode = static_cast<pattern_mode>((current_pattern_mode + change + NUMBER_OF_PATTERN_MODES) % NUMBER_OF_PATTERN_MODES);
    need_refresh = true;
  }

  if (sw == tab_object_buttonbar::rotary4) {
    if (modeselect->has_focus() || rand_timer->has_focus() || timer->has_focus() || manual_timer->has_focus()) {
      modeselect->rotary_change(change);
      rand_timer->rotary_change(change);
      timer->rotary_change(change);
      manual_timer->rotary_change(change);
    }
  }
  need_knob_refresh = true;
}

void tab_breath::switch_change(int sw, bool value) {
  need_refresh = true;

  if (sw == tab_object_buttonbar::rotary4 && value) {
    if (modeselect->has_focus()) {
      if (!modeselect->highlight_next_field()) {  // false then we left focus                          
       if (main_mode == MODE_RANDOM)
          rand_timer->highlight_next_field();
        else if (main_mode == MODE_TIMER)
          timer->highlight_next_field();
        else if (main_mode == MODE_MANUAL)
          manual_timer->highlight_next_field();
      }
    } else if (rand_timer->has_focus()) {
      rand_timer->highlight_next_field();
    } else if (timer->has_focus()) {
      timer->highlight_next_field();
    } else if (manual_timer->has_focus()) {
      manual_timer->highlight_next_field();
    } else {
      modeselect->highlight_next_field();
    }
  }

  if (main_mode == MODE_MANUAL) {
    if (sw == tab_object_buttonbar::switch1 && value && breath_block == 0) {
      int on_seconds = manual_timer->gettimeon();
      manual_on_until = millis() + (on_seconds * 1000UL);
      set_breath_block(get_breath_pc(current_pattern_mode));
      send_sync_data(SYNC_ON);
    }
    else if (sw == tab_object_buttonbar::switch1 && value && breath_block != 0) {
      manual_on_until = 0;
      set_breath_block(0);
      send_sync_data(SYNC_OFF);
    }
  }
  if ((main_mode == MODE_TIMER || main_mode == MODE_RANDOM) && sw == tab_object_buttonbar::switch1 && value) {
    if (!timed_cycle_active) {
      timed_cycle_active = true;
      manual_on_until = 0;
      modetimer->set_is_on(false);
      modetimer->start();
    } else {
      timed_cycle_active = false;
      modetimer->set_is_on(false);
      if (breath_block != 0) {
        set_breath_block(0);
        send_sync_data(SYNC_OFF);
      }
    }
  }
}

// another device can push data to us when they connect, disconnect, turn on, turn off

void tab_breath::gotsyncdata(Tab *t, sync_data syncstatus) {
  ESP_LOGD("breath", "got sync data %d from %s", syncstatus, t->gettabname());
  if (syncstatus == SYNC_ALLOFF) {
    main_mode = MODE_MANUAL;
    modeselect->reset();
    timed_cycle_active = false;
    modetimer->set_is_on(false);
    manual_on_until = 0;
    set_breath_block(0);
    need_refresh = true;
  }
}

void tab_breath::loop(bool activetab) {

  if (manual_timer) manual_timer->show(main_mode == MODE_MANUAL);

  // If we don't send a block every second or so the hardware will time out and unblock.
  // Also enforce a 2-minute maximum run time as a safety cutoff.

  if (breath_block == 100 && millis() > last_block_start + MAX_RUNTIME_MS) {
    set_breath_block(0);
    send_sync_data(SYNC_OFF);
    need_refresh = true;
  }

  if (last_block_sent != 0 && breath_block != 0) {
    if (millis() > last_block_sent + KEEPALIVE_MS) {
      send_breath_block();
    }
  }

  if (modetimer->update(timed_cycle_active && (main_mode == MODE_RANDOM || main_mode == MODE_TIMER), main_mode == MODE_RANDOM)) {
      need_refresh = true;
  }

  if (main_mode == MODE_MANUAL && breath_block != 0 && manual_on_until != 0 && millis() >= manual_on_until) {
    manual_on_until = 0;
    set_breath_block(0);
    send_sync_data(SYNC_OFF);
    need_refresh = true;
  }

  // Drive per-second refresh for manual mode countdowns
  if (activetab && main_mode == MODE_MANUAL) {
    int countdown = 0;
    if (breath_block != 0 && manual_on_until != 0 && millis() < manual_on_until) {
      countdown = (int)((manual_on_until - millis()) / 1000) + 1;
    } else if (breath_block == 100 && last_block_start != 0) {
      countdown = (int)((last_block_start + MAX_RUNTIME_MS - millis()) / 1000) + 1;
    } else if (breath_block == 0 && last_block_end != 0 && millis() < last_block_end + RECOVERY_MS) {
      countdown = (int)((last_block_end + RECOVERY_MS - millis()) / 1000) + 1;
    }
    if (countdown != last_manual_countdown) {
      last_manual_countdown = countdown;
      need_refresh = true;
    }
  }

  if (activetab && need_refresh) {
    ESP_LOGD("breath", "refresh active tab from %s on %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

    status->set_active(breath_block == 0);

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = modetimer->get_remaining_seconds();
      if (timed_cycle_active) {
        status->set_text_fmt("%.10s\n%s %d", pattern_str(current_pattern_mode), breath_block>0?"On":"Off", seconds);
      } else {
        status->set_text_fmt("%.10s\n%s", pattern_str(current_pattern_mode), breath_block>0?"On":"Off");
      }
    } else {
      // Manual mode: show max-runtime or recovery countdown if applicable
      if (breath_block != 0 && manual_on_until != 0 && millis() < manual_on_until) {
        int secs = (int)((manual_on_until - millis()) / 1000) + 1;
        status->set_text_fmt("%.10s\n%s %ds", pattern_str(current_pattern_mode), breath_block>0?"On":"Off", secs);
      } else if (breath_block != 0 && last_block_start != 0) {
        int secs = (int)((last_block_start + MAX_RUNTIME_MS - millis()) / 1000) + 1;
        status->set_text_fmt("%.10s\n%s max %ds", pattern_str(current_pattern_mode), breath_block>0?"On":"Off", secs);
      } else if (breath_block == 0 && last_block_end != 0 && millis() < last_block_end + RECOVERY_MS) {
        int secs = (int)((last_block_end + RECOVERY_MS - millis()) / 1000) + 1;
        status->set_text_fmt("%.10s\n%s wait %ds", pattern_str(current_pattern_mode), breath_block>0?"On":"Off", secs);
      } else {
        status->set_text_fmt("%.10s\n%s", pattern_str(current_pattern_mode), breath_block>0?"On":"Off");
      }
    }
    need_refresh = false;
    need_knob_refresh = true;
  }
  if (need_knob_refresh) {

    need_knob_refresh = false;

    buttonbar->set_value(tab_object_buttonbar::rotary1,0);
    if (breath_block == 0) {
      buttonbar->set_text(tab_object_buttonbar::rotary1,"Mode");
    } else {
      buttonbar->set_text(tab_object_buttonbar::rotary1,"");
    }
    
    if (main_mode == MODE_MANUAL) {
      if (breath_block > 0)
        buttonbar->set_text(tab_object_buttonbar::switch1,"Off");
      else
        buttonbar->set_text(tab_object_buttonbar::switch1,"Start");
    } else if (main_mode == MODE_TIMER || main_mode == MODE_RANDOM) {
      if (timed_cycle_active)
        buttonbar->set_text(tab_object_buttonbar::switch1,"Off");
      else
        buttonbar->set_text(tab_object_buttonbar::switch1,"Start");
      buttonbar->set_value(tab_object_buttonbar::switch1,0);
    } else {
      buttonbar->set_text(tab_object_buttonbar::switch1,"Off");
      buttonbar->set_value(tab_object_buttonbar::switch1,0);
    }

    if (main_pattern == 0 || ((main_mode == MODE_RANDOM || main_mode == MODE_TIMER) && (rand_timer->has_focus() || timer->has_focus()))) {
      buttonbar->set_value(tab_object_buttonbar::rotary4, 0);
      buttonbar->set_text(tab_object_buttonbar::rotary4, LV_SYMBOL_SETTINGS);
      if (activetab) buttonbar->set_rgb(tab_object_buttonbar::rotary4, lv_color_hsv_to_rgb(0, 0, 0));
    }
  }
}

void tab_breath::focus_change(bool focus) {
  ESP_LOGD("breath", "focus cb %s on %d: %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), focus);
  need_refresh = true;
  if (buttonbar) buttonbar->set_rgb_all(lv_color_hsv_to_rgb(0, 0, 0));
}

void tab_breath::tab_create() {
  create_standard_page(breath_main_modes_c);
  lv_obj_t *dd = modeselect->getdropdownobject();
  lv_obj_remove_event_cb_with_user_data(dd, standard_mode_change_cb, this);
  lv_obj_add_event_cb(dd, breath_mode_change_cb, LV_EVENT_VALUE_CHANGED, this);
  create_standard_widgets();
  if (manual_timer) manual_timer->view(page);

  buttonbar->set_onmain(tab_object_buttonbar::rotary1, true);
  buttonbar->set_rgb_all(lv_color_hsv_to_rgb(0, 0, 0));
}

// return false if we removed ourselves from the connected devices list
bool tab_breath::hardware_changed(void) {
  need_refresh = true;
  if (last_change == D_CONNECTING) {
    printf_log("Connecting %s\n", device->getShortName());
  } else if (last_change == D_CONNECTED) {
    tab_create();
    send_sync_data(SYNC_START);
    send_sync_data(SYNC_OFF);
    printf_log("Connected %s\n", device->getShortName());
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", device->getShortName());
    send_sync_data(SYNC_BYE);
    return false;
  }
  return true;
}