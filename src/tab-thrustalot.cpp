#include <memory>

#include "device-thrustalot.hpp"
#include "tab-thrustalot.hpp"
#include "tab.hpp"
#include "lvgl-utils.h"

tab_thrustalot::tab_thrustalot() {
  ison = true;
  lockpanel = false;
  main_mode = MODE_MANUAL;
  timer = new tab_object_timer(false);
  rand_timer = new tab_object_timer(true);
  sync = new tab_object_sync();
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
  knob_speed = 40;
  knob_tempo = 1;
}
tab_thrustalot::~tab_thrustalot() {}

void tab_thrustalot::encoder_change(int sw, int change) {
  if (main_mode == MODE_RANDOM && sw == 3) {
    rand_timer->rotary_change(change);
  }
  if (main_mode == MODE_TIMER && sw == 3) {
    timer->rotary_change(change);
  }
  if (sw == 1) {
    knob_speed = min(99,max(1,knob_speed+change));
  }
  if (sw == 0) {
    knob_tempo = min(99,max(0,knob_tempo+change));
  }
  need_knob_refresh = true;
}

void tab_thrustalot::switch_change(int sw, boolean value) {
  need_refresh = true;
  device_thrustalot *md = static_cast<device_thrustalot *>(device);
  if (main_mode == MODE_RANDOM && sw == 3 && value) {
    rand_timer->highlight_next_field();
  }
  if (main_mode == MODE_TIMER && sw == 3 && value) {
    timer->highlight_next_field();
  }
  if (main_mode == MODE_MANUAL) {
    if (sw == 4 && value && ison == 0) {
      ison = 1;
      md->thrustonetime(knob_speed);
      send_sync_data(SYNC_ON);
    } else if (sw == 4 && value && ison == 1) {
      ison = 0;
      send_sync_data(SYNC_OFF);
    }
  }
  if (sw == 2 && !ison && main_mode == MODE_MANUAL) {
    md->thrustonetime(knob_speed);
  }
}

// another device can push data to us when they connect, disconnect, turn on, turn off

void tab_thrustalot::gotsyncdata(Tab *t, sync_data syncstatus) {
  device_thrustalot *md = static_cast<device_thrustalot *>(device);
  if (!md) return;
  ESP_LOGD("thrustalot", "got sync data %d from %s", syncstatus, t->gettabname());
  if (main_mode == MODE_SYNC) {
    bool isinverted = sync->isinverted();

    if ((syncstatus == SYNC_ON && !isinverted) || (syncstatus == SYNC_OFF && isinverted)) {
      ison = true;
      md->thrustonetime(knob_speed);
    } else if ((syncstatus == SYNC_OFF && !isinverted) || (syncstatus == SYNC_ON && isinverted)) {
      ison = false;
    }
    need_refresh = true;
  }
}

int tab_thrustalot::tempo_to_ms(int tempo) {
  return tempo * 50;
}

// If ison then we want to run constantly
// so if tempo is 0, start it, and if ison is false then stop it, out
// otherwise start one thrust, when out then wait for temp, then send it in again etc
// cbpos is 1 for in, 2 for out, 0 if 'moving'

void tab_thrustalot::loop(boolean activetab) {
  device_thrustalot *md = static_cast<device_thrustalot *>(device);
  int thruststate;

  if (tempotimer !=0 && millis() >= tempotimer) {
    tempotimer = 0;
    if (ison)
      md->thrustonetime(knob_speed);
  }
  if (md && xQueueReceive(md->events,&thruststate, 0)) {
    if (thruststate == 2) thrustcount++;
    ESP_LOGI("thrust","Position is now %d", thruststate);
    if (ison && thruststate == 2) { // it's out, send it in again!
      if (knob_tempo == 0) 
        md->thrustonetime(knob_speed);
      else 
        tempotimer = millis()+ tempo_to_ms(knob_tempo);
    }
    need_refresh = true;
  }

  if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
    if (timermillis < millis()) {
      need_refresh = true;
      if (ison == 0) {
        ison = 1;
        md->thrustonetime(knob_speed);
        send_sync_data(SYNC_ON);
        if (main_mode == MODE_RANDOM)
          timermillis = millis() + rand_timer->gettimeon() * 1000;
        else
          timermillis = millis() + timer->gettimeon() * 1000;
      } else {
        ison = 0;
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
    ESP_LOGD("thrustalot", "refresh active tab from %s on %d",
             pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

    device_thrustalot *md = static_cast<device_thrustalot *>(device);
    lv_obj_set_style_bg_color(tab_status, lv_color_hex(ison?COLOUR_GREEN:COLOUR_RED), LV_PART_MAIN);

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = (timermillis - millis()) / 1000;
      lv_label_set_text_fmt(lv_obj_get_child(tab_status, 0), "%s: %s\n%d",
                             ison?"On":"Off",md->getpostext(), seconds);
    } else {
      lv_label_set_text_fmt(lv_obj_get_child(tab_status, 0), "%s: %s",
                        ison?"On":"Off",md->getpostext());
    }
    need_refresh = false;
    need_knob_refresh = true;
  }
  if (activetab && need_knob_refresh) {
    device_thrustalot *md = static_cast<device_thrustalot *>(device);

    need_knob_refresh = false;
    for (int i = 0; i < 5; i++)
      buttonbar->settext(i,"");
    buttonbar->settextfmt(0,"Speed\n%d%%",knob_speed);
    buttonbar->setvalue(0,knob_speed);
    buttonbar->setrgb(0, lv_color_hsv_to_rgb(0, 100, knob_speed));
    buttonbar->settextfmt(1,"Delay\n%.2f",(float)tempo_to_ms(knob_tempo)/1000.0);
    buttonbar->setrgb(1, lv_color_hsv_to_rgb(180, 100, (99-knob_tempo)));
    buttonbar->setvalue(1,knob_tempo);
    if (main_mode == MODE_MANUAL) {
      buttonbar->settextfmt(2,"On\nOff");
      buttonbar->setvalue(2,ison? 100:0);
    } else
      buttonbar->setvalue(2,0);
    if (!ison && main_mode == MODE_MANUAL) {
      buttonbar->settext(3,"One\nThrust");
    } else {
      buttonbar->settext(3,"");
    }

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      buttonbar->settext(4, LV_SYMBOL_BELL);
      if (main_mode == MODE_RANDOM && rand_timer->has_focus() ||
          main_mode == MODE_TIMER && timer->has_focus())
        buttonbar->setvalue(4,100);
      else
        buttonbar->setvalue(4,0);
    }

  }
}

void thrustalot_mode_change_cb(lv_event_t *event) {
  tab_thrustalot *thrustalot_tab = static_cast<tab_thrustalot *>(lv_event_get_user_data(event));
  thrustalot_tab->main_mode = static_cast<tab_thrustalot::main_modes>(lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(event)));
  ESP_LOGI("thrustalot", "cb %s on %d: new mode %d",
           pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(),
           thrustalot_tab->main_mode);
  thrustalot_tab->need_refresh = true;
  thrustalot_tab->rand_timer->show((thrustalot_tab->main_mode == tab_thrustalot::MODE_RANDOM));
  thrustalot_tab->timer->show((thrustalot_tab->main_mode == tab_thrustalot::MODE_TIMER));
  thrustalot_tab->sync->show((thrustalot_tab->main_mode == tab_thrustalot::MODE_SYNC));
}

void tab_thrustalot::focus_change(boolean focus) {
  ESP_LOGD("thrustalot", "focus cb %s on %d: %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), focus);
  need_refresh = true;
  for (int i = 0; i < 5; i++) {
      buttonbar->setrgb(i,lv_color_hsv_to_rgb(0, 0, 0));
  }
}

void tab_thrustalot::tab_create_status() {
  tab_status = lv_obj_create(page);
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

void tab_thrustalot::tab_create() {
  page = lv_tabview_add_tab(tv, gettabname());

  lv_obj_set_style_pad_left(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(page, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_right(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(page, 0, LV_PART_MAIN);

  lv_obj_t *dd = lv_dropdown_create(page);
  lv_obj_set_style_text_font(dd, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_font(lv_dropdown_get_list(dd), &lv_font_montserrat_16, LV_PART_MAIN);
  
  lv_obj_set_align(dd, LV_ALIGN_TOP_RIGHT);
  lv_obj_set_size(dd, dropdown_width, dropdown_height);  // match the timer box width

  lv_dropdown_set_options(dd, thrustalot_main_modes_c);
  lv_obj_add_event_cb(dd, thrustalot_mode_change_cb, LV_EVENT_VALUE_CHANGED, this);

  buttonbar = new tab_object_buttonbar(page);

  tab_create_status();
  rand_timer->view(page);
  timer->view(page);
  sync->view(page);

  lv_tabview_set_act(tv, lv_get_tabview_idx_from_page(tv, page), LV_ANIM_OFF);
}

// return false if we removed ourselves from the connected devices list
boolean tab_thrustalot::hardware_changed(void) {
  need_refresh = true;
  if (last_change == D_CONNECTING) {
    printf_log("Connecting %s\n", device->getShortName());
  } else if (last_change == D_CONNECTED) {
    tab_create();
    send_sync_data(SYNC_START);
    send_sync_data(SYNC_OFF);
    device_thrustalot *md = static_cast<device_thrustalot *>(device);
    xQueueReset(md->events);
    ison = false;
    printf_log("Connected %s\n", device->getShortName());
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", device->getShortName());
    send_sync_data(SYNC_BYE);
    return false;
  }
  return true;
}