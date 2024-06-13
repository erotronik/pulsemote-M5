#include "device-mk312.hpp"
#include "tab-mk312.hpp"
#include "tab.hpp"
#include "lvgl-utils.h"

tab_mk312::tab_mk312() {
  ison = true;
  lockpanel = false;
  main_mode = MODE_MANUAL;
  timer = new tab_object_timer(false);
  rand_timer = new tab_object_timer(true);
  sync = new tab_object_sync();
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
}

tab_mk312::~tab_mk312() {}

void tab_mk312::encoder_change(int sw, int change) {
  device_mk312 *md = static_cast<device_mk312 *>(device);
  if (sw == 0 && lockpanel) {
    level_b = min(99, max(0, level_b + change));
    md->etbox_setbyte(ETMEM_knobb, (level_b * 256+99) / 100);   // Round up to match the display
    need_knob_refresh = true;
  }
  if (sw == 1 && lockpanel) {
    level_a = min(99, max(0, level_a + change));
    md->etbox_setbyte(ETMEM_knoba, (level_a * 256+99) / 100);  
    need_knob_refresh = true;
  }
  if (main_mode == MODE_RANDOM && sw == 3) {
    rand_timer->rotary_change(change);
  }
  if (main_mode == MODE_TIMER && sw == 3) {
    timer->rotary_change(change);
  }
  if (sw ==2 && lockpanel) {
    for (int i=0; i< change; i++) {
      md->etbox_setbyte(ETMEM_pushbutton, ETBUTTON_lockmode);
    }
    need_refresh = true;
  }
}

void tab_mk312::switch_change(int sw, boolean value) {
  need_refresh = true;
  device_mk312 *md = static_cast<device_mk312 *>(device);
  if (main_mode == MODE_RANDOM && sw == 3 && value) {
    rand_timer->highlight_next_field();
  }
  if (main_mode == MODE_TIMER && sw == 3 && value) {
    timer->highlight_next_field();
  }
  if (main_mode == MODE_MANUAL) {
    if (sw == 4 && value && ison == 0) {
      ison = 1;
      md->etbox_on(0);
      send_sync_data(SYNC_ON);
    } else if (sw == 4 && value && ison == 1) {
      ison = 0;
      md->etbox_off();
      send_sync_data(SYNC_OFF);
    }
  }
  if (lockpanel == false && (sw == 1 || sw == 0) && value) {
    lockpanel = true;
    level_a = 0;
    level_b = 0;
    md->etbox_setbyte(ETMEM_panellock, 1);
    md->etbox_setbyte(ETMEM_knoba, level_a); // no need to scale it's 0
    md->etbox_setbyte(ETMEM_knobb, level_b);
  } else if (lockpanel == true && (sw == 1 || sw == 0) && value) {
    lockpanel = false;
    md->etbox_setbyte(ETMEM_panellock, 0);
  }
  if (lockpanel == true && sw == 2 && value && ison == 1) {
    md->etbox_setbyte(ETMEM_pushbutton, ETBUTTON_lockmode);
  }
}

// another device can push data to us when they connect, disconnect, turn on, turn off

void tab_mk312::gotsyncdata(Tab *t, sync_data syncstatus) {
  device_mk312 *md = static_cast<device_mk312 *>(device);
  if (!md) return;
  ESP_LOGD("mk312", "got sync data %d from %s", syncstatus, t->gettabname());
  if (main_mode == MODE_SYNC) {
    bool isinverted = sync->isinverted();
    if ((syncstatus == SYNC_ON && !isinverted) || (syncstatus == SYNC_OFF && isinverted)) {
      ison = true;
      md->etbox_on(0);
    } else if ((syncstatus == SYNC_OFF && !isinverted) || (syncstatus == SYNC_ON && isinverted)) {
      ison = false;
      md->etbox_off();
    }
    need_refresh = true;
  }
}

void tab_mk312::loop(boolean activetab) {
  device_mk312 *md = static_cast<device_mk312 *>(device);
  if (!md) return;

  if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
    if (timermillis < millis()) {
      need_refresh = true;
      if (ison == 0) {
        ison = 1;
        md->etbox_on(0);
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

  if (activetab && need_refresh) {
    ESP_LOGD("mk312", "refresh active tab from %s on %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

    device_mk312 *md = static_cast<device_mk312 *>(device);
    lv_obj_set_style_bg_color(tab_status, lv_color_hex(ison?COLOUR_GREEN:COLOUR_RED), LV_PART_MAIN);

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = (timermillis - millis()) / 1000;
      lv_label_set_text_fmt(lv_obj_get_child(tab_status, 0), "%s\n%d", ison?md->etmodes[md->get_mode()]:"Off", seconds);
    } else {
      lv_label_set_text(lv_obj_get_child(tab_status, 0), ison?md->etmodes[md->get_mode()]:"Off");
    }
    need_refresh = false;
    need_knob_refresh = true;
  }
  if (activetab && need_knob_refresh) {
    device_mk312 *md = static_cast<device_mk312 *>(device);

    need_knob_refresh = false;
    for (int i = 0; i < 5; i++)
      buttonbar->settext(i,"");
    if (main_mode == MODE_MANUAL) {
      buttonbar->settextfmt(2,"On\nOff");
      buttonbar->setvalue(2,ison? 100:0);
    } else
      buttonbar->setvalue(2,0);

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      buttonbar->settext(4, LV_SYMBOL_BELL);
      if (main_mode == MODE_RANDOM && rand_timer->has_focus() ||
          main_mode == MODE_TIMER && timer->has_focus())
        buttonbar->setvalue(4,100);
      else
        buttonbar->setvalue(4,0);
    }

    if (lockpanel) {
      buttonbar->setvalue(0,level_a);
      buttonbar->settextfmt(0, "A\n%" LV_PRId32 "%%", level_a);
      buttonbar->setrgb(0, lv_color_hsv_to_rgb(0, 100, level_a));
      buttonbar->setvalue(1,level_b);
      buttonbar->settextfmt(1, "B\n%" LV_PRId32 "%%", level_b);
      buttonbar->setrgb(1, lv_color_hsv_to_rgb(0, 100, level_b));
      buttonbar->settext(3, "mode");
    } else {
      buttonbar->setrgb(0, lv_color_hsv_to_rgb(0, 0, 0));
      buttonbar->setrgb(1, lv_color_hsv_to_rgb(0, 0, 0));
      buttonbar->setvalue(0,0);
      buttonbar->setvalue(1,0);
      buttonbar->settext(0, LV_SYMBOL_CHARGE);
      buttonbar->settext(1, LV_SYMBOL_CHARGE);
    }
  }
}

void mk312_mode_change_cb(lv_event_t *event) {
  tab_mk312 *mk312_tab = static_cast<tab_mk312 *>(lv_event_get_user_data(event));
  mk312_tab->main_mode = static_cast<tab_mk312::main_modes>(lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(event)));
  ESP_LOGI("mk312", "cb %s on %d: new mode %d",
           pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(),
           mk312_tab->main_mode);
  mk312_tab->need_refresh = true;
  mk312_tab->rand_timer->show((mk312_tab->main_mode == tab_mk312::MODE_RANDOM));
  mk312_tab->timer->show((mk312_tab->main_mode == tab_mk312::MODE_TIMER));
  mk312_tab->sync->show((mk312_tab->main_mode == tab_mk312::MODE_SYNC));
}

void tab_mk312::focus_change(boolean focus) {
  ESP_LOGD("mk312", "focus cb %s on %d: %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), focus);
  need_refresh = true;
  for (int i = 0; i < 5; i++) {
      buttonbar->setrgb(i,lv_color_hsv_to_rgb(0, 0, 0));
  }
}

void tab_mk312::tab_create_status(lv_obj_t *tv2) {
  tab_status = lv_obj_create(tv2);
  lv_obj_set_size(tab_status, 108, 64);
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

void tab_mk312::tab_create() {
  lv_obj_t *tv3 = lv_tabview_add_tab(tv, gettabname());

  lv_obj_set_style_pad_left(tv3, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(tv3, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_right(tv3, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(tv3, 0, LV_PART_MAIN);

  lv_obj_t *dd = lv_dropdown_create(tv3);
  lv_obj_set_style_text_font(dd, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_font(lv_dropdown_get_list(dd), &lv_font_montserrat_16, LV_PART_MAIN);

  lv_obj_set_align(dd, LV_ALIGN_TOP_RIGHT);
  lv_obj_set_size(dd, dropdown_width, dropdown_height);  // match the timer box width

  lv_dropdown_set_options(dd, mk312_main_modes_c);
  lv_obj_add_event_cb(dd, mk312_mode_change_cb, LV_EVENT_VALUE_CHANGED, this);

  buttonbar = new tab_object_buttonbar(tv3);

  tab_create_status(tv3);

  rand_timer->view(tv3);
  timer->view(tv3);
  sync->view(tv3);

  page = tv3;

  lv_tabview_set_act(tv, lv_get_tabview_idx_from_page(tv, tv3), LV_ANIM_OFF);
}

// return false if we removed ourselves from the connected devices list
boolean tab_mk312::hardware_changed(void) {
  ESP_LOGD("mk312","hardware changed %d", last_change);
  need_refresh = true;
  if (last_change == D_CONNECTING) {
    printf_log("Connecting %s\n", device->getShortName());
  } else if (last_change == D_CONNECTED) {
    device_mk312 *md = static_cast<device_mk312 *>(device);
    tab_create();
    send_sync_data(SYNC_START);
    send_sync_data(SYNC_ON);
    md->etbox_on(0);
    printf_log("Connected %s\n", device->getShortName());
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", device->getShortName());
    send_sync_data(SYNC_BYE);
    return false;
  }
  return true;
}