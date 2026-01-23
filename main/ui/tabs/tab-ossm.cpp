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
  modetimer = new tab_object_modetimer(timer, rand_timer, [this](bool active_on) {
      device_ossm *md = static_cast<device_ossm *>(device);
      ison = active_on;
      if (active_on) {
          md->set_speed(knob_speed);
          send_sync_data(SYNC_ON);
      } else {
          md->set_speed(0);
          send_sync_data(SYNC_OFF);
      }
  });
  page = nullptr;
  old_last_change = last_change = D_NONE;
  status = nullptr;
  segbar = nullptr;
  device = nullptr;
  knob_speed = 0;
  knob_stroke = 0;
  knob_sensation = 0;
  knob_depth = 0;
  main_pattern = 0; // continuous
}
tab_ossm::~tab_ossm() {}

void tab_ossm::encoder_change(int sw, int change) {
  device_ossm *md = static_cast<device_ossm *>(device);

  if (sw == tab_object_buttonbar::rotary4) { // this rotary control gets reused depending on context
    if (modeselect->has_focus() || rand_timer->has_focus() || timer->has_focus()) {
      modeselect->rotary_change(change);
      rand_timer->rotary_change(change);
      timer->rotary_change(change);
    } else if (main_pattern !=0) {
      knob_sensation = std::min(100,std::max(0,knob_sensation+change*4));
      md->set_sensation(knob_sensation);      
    }
  }
  if (sw == tab_object_buttonbar::rotary1) {
    knob_speed = std::min(100,std::max(0,knob_speed+change*2));
    if (ison) md->set_speed(knob_speed);
  }
  if (sw == tab_object_buttonbar::rotary2) {
    knob_stroke = std::min(100,std::max(0,knob_stroke+change*4));
    md->set_stroke(knob_stroke);
  }
  if (sw == tab_object_buttonbar::rotary3 ) {
    knob_depth = std::min(100,std::max(0,knob_depth+change*4));
    md->set_depth(knob_depth);
  }
  need_knob_refresh = true;
}

void tab_ossm::switch_change(int sw, bool value) {
  need_refresh = true;
  device_ossm *md = static_cast<device_ossm *>(device);

  if (sw == tab_object_buttonbar::rotary1 && value) {
    main_pattern++;
    if (!strcmp(md->pattern_name_for_idx(main_pattern),"")) 
      main_pattern = 0;
    md->set_pattern(main_pattern);
  }
  if (sw == tab_object_buttonbar::rotary2 && value) {
    main_pattern = 0;
    md->set_pattern(main_pattern);
  }

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
      md->set_speed(knob_speed);
      send_sync_data(SYNC_ON);
    } else if (sw == tab_object_buttonbar::switch1 && value && ison == 1) {
      ison = false;
      md->set_speed(0);
      send_sync_data(SYNC_OFF);
    }
  }
  if (main_mode != MODE_MANUAL && sw == tab_object_buttonbar::switch1 && value) {
    if (ison) {
      ison = false;
      md->set_speed(0);
      send_sync_data(SYNC_OFF);
    }
    main_mode = MODE_MANUAL;
    modetimer->set_is_on(false);
    modeselect->reset();
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
    md->set_speed(0);
    need_refresh = true;
  }
  if (main_mode == MODE_SYNC) {
    bool isinverted = sync->isinverted();

  if ((syncstatus == SYNC_ON && !isinverted) || (syncstatus == SYNC_OFF && isinverted)) {
      ison = true;
      md->set_speed(knob_speed);
    } else if ((syncstatus == SYNC_OFF && !isinverted) || (syncstatus == SYNC_ON && isinverted)) {
      ison = false;
      md->set_speed(0);
    }
    need_refresh = true;
  }
}

void tab_ossm::loop(bool activetab) {
  device_ossm *md = static_cast<device_ossm *>(device);

  if (!firstdata) {
    if (md->got_data_yet()) {
      firstdata = true;
      knob_speed = md->get_speed();
      knob_depth = md->get_depth();
      knob_stroke = md->get_stroke();
      knob_sensation = md->get_sensation();
      main_pattern = md->get_pattern();
      need_refresh = true;
    }
  }

  if (modetimer->update(main_mode == MODE_RANDOM || main_mode == MODE_TIMER, main_mode == MODE_RANDOM)) {
      need_refresh = true;
  }

  if (activetab && need_refresh) {
    ESP_LOGD("ossm", "refresh active tab from %s on %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

    device_ossm *md = static_cast<device_ossm *>(device);
    status->set_active(ison);

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = modetimer->get_remaining_seconds();
      status->set_text_fmt("%.10s\n%s: %d", md->pattern_name_for_idx(main_pattern), ison?"On":"Off", seconds);
    } else {
      status->set_text_fmt("%.10s\n%s", md->pattern_name_for_idx(main_pattern), ison?"On":"Off");
    }
    need_refresh = false;
    need_knob_refresh = true;
  }
  if (need_knob_refresh) {
    need_knob_refresh = false;
 
    buttonbar->set_text_fmt(tab_object_buttonbar::rotary1,"Speed\n%d%%",knob_speed);
    buttonbar->set_value(tab_object_buttonbar::rotary1,knob_speed);
    buttonbar->set_ison(tab_object_buttonbar::rotary1, ison);
    if (activetab) buttonbar->set_rgb(tab_object_buttonbar::rotary1, ison?lv_color_hsv_to_rgb(0, 100, knob_speed): lv_color_hsv_to_rgb(0, 0, 0));

    buttonbar->set_text_fmt(tab_object_buttonbar::rotary2,"Stroke\n%d%%",knob_stroke);
    buttonbar->set_value(tab_object_buttonbar::rotary2,knob_stroke);
    if (activetab)  buttonbar->set_rgb(tab_object_buttonbar::rotary2, lv_color_hsv_to_rgb(60, 100, knob_stroke));

    buttonbar->set_text_fmt(tab_object_buttonbar::rotary3,"Depth\n%d%%",knob_depth);
    buttonbar->set_value(tab_object_buttonbar::rotary3,knob_depth);
    if (activetab)  buttonbar->set_rgb(tab_object_buttonbar::rotary3, lv_color_hsv_to_rgb(120, 100, knob_depth));

    buttonbar->set_click_text(tab_object_buttonbar::rotary1,"patn");
    buttonbar->set_click_text(tab_object_buttonbar::rotary2,main_pattern == 0? "" : "reset");

    if (activetab) segbar->set_range(knob_depth - knob_stroke, knob_depth);

    if (main_mode == MODE_MANUAL) {
      buttonbar->set_text(tab_object_buttonbar::switch1,"On\nOff");
      buttonbar->set_value(tab_object_buttonbar::switch1,ison? 100:0);
    } else {
      buttonbar->set_text(tab_object_buttonbar::switch1,"Stop");
      buttonbar->set_value(tab_object_buttonbar::switch1,0);
    }

    if (main_pattern == 0 || ((main_mode == MODE_RANDOM || main_mode == MODE_TIMER) && (rand_timer->has_focus() || timer->has_focus()))) {
      buttonbar->set_value(tab_object_buttonbar::rotary4, 0);
      buttonbar->set_text(tab_object_buttonbar::rotary4, LV_SYMBOL_SETTINGS);
      if (activetab) buttonbar->set_rgb(tab_object_buttonbar::rotary4, lv_color_hsv_to_rgb(0, 0, 0));
    } else {
      buttonbar->set_text_fmt(tab_object_buttonbar::rotary4,"Sens\n%d%%",knob_sensation);
      buttonbar->set_value(tab_object_buttonbar::rotary4,knob_sensation);
      if (activetab) buttonbar->set_rgb(tab_object_buttonbar::rotary4, lv_color_hsv_to_rgb(180, 100, knob_sensation));
    }
  }
}

void ossm_mode_change_cb(lv_event_t *event) {
  tab_ossm *ossm_tab = static_cast<tab_ossm *>(lv_event_get_user_data(event));
  ossm_tab->main_mode = static_cast<tab_ossm::main_modes>(lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(event)));
  ESP_LOGI("ossm", "cb %s on %d: new mode %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), ossm_tab->main_mode);
  ossm_tab->need_refresh = true;
  if (ossm_tab->main_mode == tab_ossm::MODE_RANDOM || ossm_tab->main_mode == tab_ossm::MODE_TIMER) {
      ossm_tab->modetimer->start();
  }
  ossm_tab->rand_timer->show((ossm_tab->main_mode == tab_ossm::MODE_RANDOM));
  ossm_tab->timer->show((ossm_tab->main_mode == tab_ossm::MODE_TIMER));
  ossm_tab->sync->show((ossm_tab->main_mode == tab_ossm::MODE_SYNC));
}

void tab_ossm::focus_change(bool focus) {
  ESP_LOGD("ossm", "focus cb %s on %d: %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), focus);
  need_refresh = true;
  buttonbar->set_rgb_all(lv_color_hsv_to_rgb(0, 0, 0));
}


void tab_ossm::tab_create() {
  page = lv_tabview_add_tab(tv, gettabname());
 
  lv_obj_add_style(page, &lvpulsemote_style_tab, LV_PART_MAIN);

  modeselect->createdropdown(page, ossm_main_modes_c);
  lv_obj_add_event_cb(modeselect->getdropdownobject(), ossm_mode_change_cb, LV_EVENT_VALUE_CHANGED, this);

  buttonbar = new tab_object_buttonbar(page);
  buttonbar->set_onmain(tab_object_buttonbar::rotary1, true);

  status = new tab_object_status(page, 150, 64);
  status->align(LV_ALIGN_TOP_LEFT, LV_SCALE(4), 0);

  segbar = new tab_object_segbar(page, LV_SCALE(150), LV_SCALE(12));
  segbar->align(LV_ALIGN_TOP_LEFT, LV_SCALE(4), LV_SCALE(72));

  rand_timer->view(page);
  timer->view(page);
  sync->view(page);

  lv_tabview_set_act(tv, lv_get_tabview_idx_from_page(tv, page), LV_ANIM_OFF);
}


// return false if we removed ourselves from the connected devices list
bool tab_ossm::hardware_changed(void) {
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