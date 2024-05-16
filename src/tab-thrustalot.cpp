#include <memory>
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>

#include "device-thrustalot.hpp"
#include "hsv.h"
#include "tab-thrustalot.hpp"
#include "tab-object-timer.hpp"
#include "tab.hpp"
#include "lvgl-utils.h"
#include "buttonbar.hpp"

const char *thrustalot_main_modes_c =
    "Manual\nTimer\nRandom\nSync";

tab_thrustalot::tab_thrustalot() {
  ison = true;
  lockpanel = false;
  main_mode = MODE_MANUAL;
  timer = new tab_object_timer(false);
  rand_timer = new tab_object_timer(true);
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
}
tab_thrustalot::~tab_thrustalot() {}

void tab_thrustalot::encoder_change(int sw, int change) {
  ESP_LOGD("thrustalot", "encoder change and tab stuff %d", ison);

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
      md->thrustallthewayin();
      send_sync_data(SYNC_ON);
    } else if (sw == 4 && value && ison == 1) {
      ison = 0;
      md->thrustallthewayout();
      send_sync_data(SYNC_OFF);
    }
  }
  if (sw == 0) {
    md->thrustonetime(31);
  }
}

void tab_thrustalot::send_sync_data(sync_data syncstatus) {
  for (int i=0; i<tabs.size(); i++) {
    Tab *st = tabs.get(i);
    if (st != this) {
      st->gotsyncdata(this,syncstatus);
    }
  }
}

// another device can push data to us when they connect, disconnect, turn on, turn off

void tab_thrustalot::gotsyncdata(Tab *t, sync_data syncstatus) {
  ESP_LOGD("thrustalot", "got sync data %d from %s\n", syncstatus, t->device->getShortName());
  if (main_mode == MODE_SYNC) {
    device_thrustalot *md = static_cast<device_thrustalot *>(device);
    if (syncstatus == SYNC_ON) {
      ison = true;
    } else if (syncstatus == SYNC_OFF) {
      ison = false;
    }
    need_refresh = true;
  }
}

void tab_thrustalot::loop(boolean activetab) {
  device_thrustalot *md = static_cast<device_thrustalot *>(device);

  if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
    if (timermillis < millis()) {
      need_refresh = true;
      if (ison == 0) {
        ison = 1;
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
    lv_obj_set_style_bg_color(tab_status, lv_color_hex(ison?COLOUR_GREEN:COLOUR_RED),
                                LV_PART_MAIN);

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = (timermillis - millis()) / 1000;
      lv_label_set_text_fmt(lv_obj_get_child(tab_status, 0), "%s\n%d",
                             ison?"On":"Off", seconds);
    } else {
      lv_label_set_text(lv_obj_get_child(tab_status, 0),
                        ison?"On":"Off");
    }
    need_refresh = false;
    need_knob_refresh = true;
  }
  if (activetab && need_knob_refresh) {
    device_thrustalot *md = static_cast<device_thrustalot *>(device);

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
      if (main_mode == MODE_RANDOM && rand_timer->has_active_button() ||
          main_mode == MODE_TIMER && timer->has_active_button())
        buttonbar->setvalue(4,100);
      else
        buttonbar->setvalue(4,0);
    }

  }
}

void thrustalot_mode_change_cb(lv_event_t *event) {
  tab_thrustalot *thrustalot_tab =
      static_cast<tab_thrustalot *>(lv_event_get_user_data(event));
  thrustalot_tab->main_mode = static_cast<tab_thrustalot::main_modes>(
      lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(event)));
  ESP_LOGI("thrustalot", "cb %s on %d: new mode %d",
           pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(),
           thrustalot_tab->main_mode);
  thrustalot_tab->need_refresh = true;
  thrustalot_tab->rand_timer->show((thrustalot_tab->main_mode == tab_thrustalot::MODE_RANDOM));
  thrustalot_tab->timer->show((thrustalot_tab->main_mode == tab_thrustalot::MODE_TIMER));
}

void tab_thrustalot::focus_change(boolean focus) {
  ESP_LOGD("thrustalot", "focus cb %s on %d: %d",
           pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), focus);
  need_refresh = true;
  for (int i = 0; i < 5; i++) {
      buttonbar->setrgb(i,hsvToRgb(0, 0, 0));
  }
}

void tab_thrustalot::tab_create_status(lv_obj_t *tv2) {
  lv_obj_t *square = lv_obj_create(tv2);
  lv_obj_set_size(square, 108, 64);
  lv_obj_align(square, LV_ALIGN_TOP_LEFT, 4, 0);
  lv_obj_set_style_bg_color(square, lv_color_hex(0xFF0000), LV_PART_MAIN);
  lv_obj_t *labelx = lv_label_create(square);
  lv_label_set_text(labelx, "-");
  lv_obj_set_style_text_font(labelx, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_obj_set_style_text_align(labelx, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_pad_top(square, 3, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(square, 3, LV_PART_MAIN);
  lv_obj_align(labelx, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_t *extra_label = lv_label_create(square);
  lv_obj_set_style_text_font(extra_label, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_label_set_text(extra_label, "");
  lv_obj_set_style_text_align(extra_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(extra_label, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_scrollbar_mode(square, LV_SCROLLBAR_MODE_OFF);
  tab_status = square;
}

void tab_thrustalot::tab_create() {
  lv_obj_t *tv3;
  device_mk312 *md = static_cast<device_mk312 *>(device);

  tv3 = lv_tabview_add_tab(tv, md->getShortName());

  lv_obj_set_style_pad_left(tv3, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(tv3, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_right(tv3, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(tv3, 0, LV_PART_MAIN);

  lv_obj_t *dd = lv_dropdown_create(tv3);
  lv_obj_set_style_text_font(dd, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_align(dd, LV_ALIGN_TOP_RIGHT);
  lv_obj_set_size(dd, 160, 36);  // match the timer box width

  lv_dropdown_set_options(dd, thrustalot_main_modes_c);
  lv_obj_add_event_cb(dd, thrustalot_mode_change_cb, LV_EVENT_VALUE_CHANGED, this);

  buttonbar = new ButtonBar(tv3);

  tab_create_status(tv3);

  lv_obj_t *lt = rand_timer->view(tv3);
  lv_obj_align(lt, LV_ALIGN_TOP_RIGHT, 0, 40);
  rand_timer->show(false);

  lt = timer->view(tv3);
  lv_obj_align(lt, LV_ALIGN_TOP_RIGHT, 0, 40);
  timer->show(false);

  page = tv3;

  lv_tabview_set_act(tv, lv_get_tabview_idx_from_page(tv, tv3), LV_ANIM_OFF);
}

// return false if we removed ourselves from the connected devices list
boolean tab_thrustalot::hardware_changed(void) {
  need_refresh = true;
  if (last_change == D_CONNECTING) {
    printf_log("Connecting %s\n", device->getShortName());
  } else if (last_change == D_CONNECTED) {
    tab_create();
    send_sync_data(SYNC_START);
    send_sync_data(SYNC_ON);
    printf_log("Connected %s\n", device->getShortName());
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", device->getShortName());
    return false;
  }
  return true;
}