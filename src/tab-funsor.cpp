#include <memory>
#include "device-funosr.hpp"
#include "tab-funosr.hpp"
#include "tab.hpp"
#include "lvgl-utils.h"

tab_funosr::tab_funosr() {
  ison = false;
  main_mode = MODE_MANUAL;
  timer = new tab_object_timer(false);
  rand_timer = new tab_object_timer(true);
  sync = new tab_object_sync();
  modeselect = new tab_object_modes();
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
  knob_speed = 2;
  knob_stroke = 20;
  knob_depth = 100;
}
tab_funosr::~tab_funosr() {}

void tab_funosr::send_funosr() {
  device_funosr *md = static_cast<device_funosr *>(device);
  if (!md) return;
  int smax = min(99,100- knob_depth);
  int smin = min(99,max(0,100 - (knob_depth - knob_stroke)));
  int duration = (int)(100 * pow((5000.0 / 100.0), (100.0 - knob_speed) / 100.0));
  // — smaller denominators (like 70.0) make it fall faster; larger flatten it out.
  sprintf(msg,"minmax %d %d %d", smin, smax, duration);
  ESP_LOGD("stroke","sending %s",msg);
  md->funosr_stroke(smin, smax, duration);
}

void tab_funosr::send_stop() {
  goto_max();
  //mqttsend(mqtt_topic,"stop");
}

void tab_funosr::goto_min() {
  device_funosr *md = static_cast<device_funosr *>(device);
  if (!md) return;
  md->funosr_goto(min(99,max(0,100- (knob_depth - knob_stroke))));
}

void tab_funosr::goto_max() {
  device_funosr *md = static_cast<device_funosr *>(device);
  if (!md) return;
  md->funosr_goto(min(99,100 - knob_depth));
}

void tab_funosr::encoder_change(int sw, int change) {
  if (sw == tab_object_buttonbar::rotary4) { // this rotary control gets reused depending on context
    if (modeselect->has_focus() || rand_timer->has_focus() || timer->has_focus()) {
      modeselect->rotary_change(change);
      rand_timer->rotary_change(change);
      timer->rotary_change(change);
    }
  }
  if (sw == tab_object_buttonbar::rotary1) {
    knob_speed = min(100,max(0,knob_speed+change*2));
    if (knob_speed <2) knob_speed = 2;
    if (ison) send_funosr();
  }
  if (sw == tab_object_buttonbar::rotary2) {
    knob_stroke = min(100,max(0,knob_stroke+change*4));
    if (knob_depth < knob_stroke) {
      knob_depth = knob_stroke;
    }
    if (ison) send_funosr();
  }
  if (sw == tab_object_buttonbar::rotary3 ) {
    knob_depth = min(100,max(0,knob_depth+change*4));
    if (knob_depth < knob_stroke) {
      knob_stroke = knob_depth;
    }
    if (ison) send_funosr();
  }
  need_knob_refresh = true;
}

void tab_funosr::switch_change(int sw, boolean value) {
  need_refresh = true;

  if (sw == tab_object_buttonbar::rotary2 && value) {
    goto_min();
    ison = false;
    main_mode = MODE_MANUAL;
    modeselect->reset();
    send_sync_data(SYNC_OFF);

  }
  if (sw == tab_object_buttonbar::rotary3 && value) {
    goto_max();
    ison = false;
    main_mode = MODE_MANUAL;
    modeselect->reset();
    send_sync_data(SYNC_OFF);
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
      send_funosr();
      send_sync_data(SYNC_ON);
    } else if (sw == tab_object_buttonbar::switch1 && value && ison == 1) {
      ison = false;
      send_stop();
      send_sync_data(SYNC_OFF);
    }
  }
  if (main_mode != MODE_MANUAL && sw == tab_object_buttonbar::switch1 && value) {
    if (ison) {
      ison = false;
      send_stop();
      send_sync_data(SYNC_OFF);
    }
    main_mode = MODE_MANUAL;
    modeselect->reset();
  }
}

// another device can push data to us when they connect, disconnect, turn on, turn off

void tab_funosr::gotsyncdata(Tab *t, sync_data syncstatus) {
  ESP_LOGD("funosr", "got sync data %d from %s", syncstatus, t->gettabname());
  if (syncstatus == SYNC_ALLOFF) {
    main_mode = MODE_MANUAL;
    modeselect->reset();
    ison = false;
    send_stop();
    need_refresh = true;
  }
  if (main_mode == MODE_SYNC) {
    bool isinverted = sync->isinverted();

  if ((syncstatus == SYNC_ON && !isinverted) || (syncstatus == SYNC_OFF && isinverted)) {
      ison = true;
      send_funosr();
    } else if ((syncstatus == SYNC_OFF && !isinverted) || (syncstatus == SYNC_ON && isinverted)) {
      ison = false;
      send_stop();
    }
    need_refresh = true;
  }
}

void tab_funosr::loop(boolean activetab) {

  if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
    if (timermillis < millis()) {
      need_refresh = true;
      if (!ison) {
        ison = true;
        send_funosr();
        send_sync_data(SYNC_ON);
        if (main_mode == MODE_RANDOM)
          timermillis = millis() + rand_timer->gettimeon() * 1000;
        else
          timermillis = millis() + timer->gettimeon() * 1000;
      } else {
        ison = false;
        send_stop();
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
    ESP_LOGD("funosr", "refresh active tab from %s on %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

    lv_obj_set_style_bg_color(tab_status, lv_color_hex(ison?COLOUR_GREEN:COLOUR_RED), LV_PART_MAIN);

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = (timermillis - millis()) / 1000;
      lv_label_set_text_fmt(lv_obj_get_child(tab_status, 0), "%.10s\n%s: %d", "Stroke", ison?"On":"Off", seconds);
    } else {
      lv_label_set_text_fmt(lv_obj_get_child(tab_status, 0), "%.10s\n%s", "Stroke", ison?"On":"Off");
    }
    need_refresh = false;
    need_knob_refresh = true;
  }
  if (activetab && need_knob_refresh) {

    need_knob_refresh = false;
 
    buttonbar->set_text_fmt(tab_object_buttonbar::rotary1,"Speed\n%d%%",knob_speed);
    buttonbar->set_value(tab_object_buttonbar::rotary1,knob_speed);
    buttonbar->set_rgb(tab_object_buttonbar::rotary1, ison?lv_color_hsv_to_rgb(0, 100, knob_speed): lv_color_hsv_to_rgb(0, 0, 0));

    buttonbar->set_text_fmt(tab_object_buttonbar::rotary2,"Stroke\n%d%%",knob_stroke);
    buttonbar->set_value(tab_object_buttonbar::rotary2,knob_stroke);
    buttonbar->set_rgb(tab_object_buttonbar::rotary2, lv_color_hsv_to_rgb(60, 100, knob_stroke));

    buttonbar->set_text_fmt(tab_object_buttonbar::rotary3,"Depth\n%d%%",knob_depth);
    buttonbar->set_value(tab_object_buttonbar::rotary3,knob_depth);
    buttonbar->set_rgb(tab_object_buttonbar::rotary3, lv_color_hsv_to_rgb(120, 100, knob_depth));

    segbar_set(&mybar, knob_depth-knob_stroke, knob_depth);
    buttonbar->set_click_text(tab_object_buttonbar::rotary3,"Go In");
    buttonbar->set_click_text(tab_object_buttonbar::rotary2,"Go Out");

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
      buttonbar->set_rgb(tab_object_buttonbar::rotary4, lv_color_hsv_to_rgb(0, 0, 0));
    }
  }
}

void funosr_mode_change_cb(lv_event_t *event) {
  tab_funosr *funosr_tab = static_cast<tab_funosr *>(lv_event_get_user_data(event));
  funosr_tab->main_mode = static_cast<tab_funosr::main_modes>(lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(event)));
  ESP_LOGI("funosr", "cb %s on %d: new mode %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), funosr_tab->main_mode);
  funosr_tab->need_refresh = true;
  funosr_tab->rand_timer->show((funosr_tab->main_mode == tab_funosr::MODE_RANDOM));
  funosr_tab->timer->show((funosr_tab->main_mode == tab_funosr::MODE_TIMER));
  funosr_tab->sync->show((funosr_tab->main_mode == tab_funosr::MODE_SYNC));
}

void tab_funosr::focus_change(boolean focus) {
  ESP_LOGD("funosr", "focus cb %s on %d: %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), focus);
  need_refresh = true;
  if (buttonbar) buttonbar->set_rgb_all(lv_color_hsv_to_rgb(0, 0, 0));
}

void tab_funosr::tab_create_status(lv_obj_t *tv2) {
  tab_status = lv_obj_create(tv2);

  lv_obj_add_style(tab_status, &lvpulsemote_style_status, LV_PART_MAIN);
  lv_obj_set_size(tab_status, 150, 64);
  lv_obj_align(tab_status, LV_ALIGN_TOP_LEFT, 4, 0);
  lv_obj_set_scrollbar_mode(tab_status, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *labelx = lv_label_create(tab_status);
  lv_label_set_text(labelx, "-");
  lv_obj_align(labelx, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *extra_label = lv_label_create(tab_status);
  lv_label_set_text(extra_label, "");
  lv_obj_align(extra_label, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void tab_funosr::tab_create() {
  ESP_LOGD("funosr","tab_create");

  page = lv_tabview_add_tab(tv, gettabname());
 
  lv_obj_add_style(page, &lvpulsemote_style_tab, LV_PART_MAIN);

  modeselect->createdropdown(page, funosr_main_modes_c);
  lv_obj_add_event_cb(modeselect->getdropdownobject(), funosr_mode_change_cb, LV_EVENT_VALUE_CHANGED, this);

  buttonbar = new tab_object_buttonbar(page);

  tab_create_status(page);
  segbar_create(page,&mybar,150,12);
  rand_timer->view(page);
  timer->view(page);
  sync->view(page);

  lv_tabview_set_act(tv, lv_get_tabview_idx_from_page(tv, page), LV_ANIM_OFF);
}

static inline void clamp_and_order(int *x, int *y, int minv, int maxv) {
  if (*x < minv) *x = minv;
  if (*x > maxv) *x = maxv;
  if (*y < minv) *y = minv;
  if (*y > maxv) *y = maxv;
  if (*y < *x) { int tmp = *x; *x = *y; *y = tmp; }
}

void tab_funosr::segbar_create(lv_obj_t *parent, segbar_t *bar, int w, int h) {
  bar->width  = w;
  bar->height = h;

  // green base
  bar->base = lv_obj_create(parent);
  lv_obj_remove_style_all(bar->base);                  
  lv_obj_set_size(bar->base, bar->width, bar->height);
  lv_obj_set_style_bg_color(bar->base, lv_color_hex(COLOUR_GREEN), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar->base, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(bar->base, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(bar->base, 0, LV_PART_MAIN);
  lv_obj_remove_flag(bar->base, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(bar->base, LV_ALIGN_TOP_LEFT, 4, 72);

  // red segment (child)
  bar->seg = lv_obj_create(bar->base);
  lv_obj_remove_style_all(bar->seg);
  lv_obj_set_style_bg_color(bar->seg, lv_color_hex(COLOUR_RED), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar->seg, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(bar->seg, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(bar->seg, 0, LV_PART_MAIN);
  lv_obj_remove_flag(bar->seg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(bar->seg, LV_OBJ_FLAG_FLOATING);        // stay on top / ignore layouts
  segbar_set(bar, 0, 0);
}

void tab_funosr::segbar_set(segbar_t *bar, int x, int y) {
  int xs = x * bar->width /100, ys = y * bar->width/100;
  clamp_and_order(&xs, &ys, 0, bar->width);
  int w = ys - xs;
  if (w < 0) w = 0;
  lv_obj_set_pos(bar->seg, xs, 0);
  lv_obj_set_size(bar->seg, w, bar->height);
}

// return false if we removed ourselves from the connected devices list
boolean tab_funosr::hardware_changed(void) {
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