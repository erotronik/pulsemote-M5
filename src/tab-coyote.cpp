#include <memory>
#include "device-coyote2.hpp"
#include "lvgl-utils.h"
#include "tab.hpp"
#include "tab-coyote.hpp"
#include "lvgl-utils.h"

tab_coyote::tab_coyote() {
    page = nullptr;
    old_last_change = last_change = D_NONE;
    main_mode = MODE_MANUAL;
    timer = new tab_object_timer(false);
    rand_timer = new tab_object_timer(true);
    sync = new tab_object_sync();
    modeselect = new tab_object_modes();
    device = nullptr;
    bool need_refresh =false;
    ison = true;
}
tab_coyote::~tab_coyote() {}

void tab_coyote::gotsyncdata(Tab *t, sync_data syncstatus) {
  device_coyote2 *md = static_cast<device_coyote2*>(device);
  if (!md) return;
  ESP_LOGD("coyote", "got sync data %d from %s", syncstatus, t->gettabname());
  if (syncstatus == SYNC_ALLOFF) {
    main_mode = MODE_MANUAL;
    modeselect->reset();
    ison = false;
    md->get().chan_a().put_setmode(M_NONE);
    md->get().chan_b().put_setmode(M_NONE); 
  }
  if (main_mode == MODE_SYNC) {
    bool isinverted = sync->isinverted();
    if ((syncstatus == SYNC_ON && !isinverted) || (syncstatus == SYNC_OFF && isinverted)) {
      ison = true;
      md->get().chan_a().put_setmode(mode_a);
      md->get().chan_b().put_setmode(mode_b); 
    } else if ((syncstatus == SYNC_OFF && !isinverted) || (syncstatus == SYNC_ON && isinverted)) {
      ison = false;
      md->get().chan_a().put_setmode(M_NONE);
      md->get().chan_b().put_setmode(M_NONE); 
    }
    need_refresh = true;
  }
}

void tab_coyote::switch_change(int sw, boolean state) {
  need_refresh = true;

  if (sw == 3 && state) {
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
    device_coyote2 *md = static_cast<device_coyote2*>(device);
    if (sw == 4 && state && ison == 0) {
      ison = 1;
      md->get().chan_a().put_setmode(mode_a);
      md->get().chan_b().put_setmode(mode_b); 
      send_sync_data(SYNC_ON);
    } else if (sw == 4 && state && ison == 1) {
      ison = 0;
      md->get().chan_a().put_setmode(M_NONE);
      md->get().chan_b().put_setmode(M_NONE);
      send_sync_data(SYNC_OFF);
    }
  }

  if (sw == 1 || sw ==0) { // click to move to the next mode, then back to start
    device_coyote2 *md = static_cast<device_coyote2*>(device);
    coyote_mode mode;
    if (sw == 1) 
      mode = md->get().chan_a().get_mode();
    else 
      mode = md->get().chan_b().get_mode();
    int i = 0;
    while (md->modes[i] != M_NONE && md->modes[i] != mode) {
      i++;
    }
    if (md->modes[i] == M_NONE || md->modes[i+1] == M_NONE) {
      i = 0;
    } else {
      i++;
    }
    if (sw == 1)  {
      md->get().chan_a().put_setmode(md->modes[i]);
      mode_a = md->modes[i];
    } else {
      md->get().chan_b().put_setmode(md->modes[i]);
      mode_b = md->modes[i];
    }
  }
}

void tab_coyote::encoder_change(int sw, int change) {
  device_coyote2 *md = static_cast<device_coyote2*>(device);
  need_refresh = true;

  if (sw == 1) 
    md->get().chan_a().put_power_diff(change);
  else if (sw == 0)
    md->get().chan_b().put_power_diff(change); 
  if (sw == 3) {
    rand_timer->rotary_change(change);
    timer->rotary_change(change);
    modeselect->rotary_change(change);
  }
}

void tab_coyote::focus_change(boolean focus) {
  for (int i=0; i<5; i++)
    buttonbar->setrgb(i, lv_color_hsv_to_rgb(0, 0, 0));
  buttonbar->settext(4, LV_SYMBOL_SETTINGS);
  need_refresh = true;
}

void tab_coyote::loop(bool active) {
  if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {

    if (timermillis < millis()) {
      device_coyote2 *md = static_cast<device_coyote2*>(device);
      need_refresh = true;
      if (ison == 0) {
        ison = 1;
        md->get().chan_a().put_setmode(mode_a);
        md->get().chan_b().put_setmode(mode_b);
        send_sync_data(SYNC_ON);
        if (main_mode == MODE_RANDOM)
          timermillis = millis() + rand_timer->gettimeon() * 1000;
        else
          timermillis = millis() + timer->gettimeon() * 1000;
      } else {
        ison = 0;
        md->get().chan_a().put_setmode(M_NONE);
        md->get().chan_b().put_setmode(M_NONE);   
        send_sync_data(SYNC_OFF);
        if (main_mode == MODE_RANDOM)
          timermillis = millis() + rand_timer->gettimeoff() * 1000;
        else
          timermillis = millis() + timer->gettimeoff() * 1000;
      }
    }
    if (active) {
      int seconds = (timermillis - millis()) / 1000;
      static int last_seconds;
      if (seconds != last_seconds) {
        need_refresh = true;
        last_seconds = seconds;
      }
    }
  }

  if (active && need_refresh) {
    device_coyote2 *md = static_cast<device_coyote2*>(device);
    need_refresh = false;

    int power = md->get().chan_a().get_power_pc();
    buttonbar->setvalue(0, power); 
    buttonbar->settextfmt(0, "A\n%" LV_PRId32 "%%", power);
    buttonbar->setrgb(0, lv_color_hsv_to_rgb(0, 100, power));

    power = md->get().chan_b().get_power_pc();
    buttonbar->setvalue(1, power); 
    buttonbar->settextfmt(1, "B\n%" LV_PRId32 "%%", power);
    buttonbar->setrgb(1, lv_color_hsv_to_rgb(0, 100, power));

    if (ison) {
      mode_a = md->get().chan_a().get_mode();
      mode_b = md->get().chan_b().get_mode();
    } 
    lv_obj_set_style_bg_color(tab_status, lv_color_hex(ison?COLOUR_GREEN:COLOUR_RED), LV_PART_MAIN);

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = (timermillis - millis()) / 1000;
      lv_label_set_text_fmt(lv_obj_get_child(tab_status, 0), "A: %s\nB: %s\n%d",
                              md->getModeName(ison?mode_a:M_NONE),
                              md->getModeName(ison?mode_b:M_NONE),
                              seconds);
    } else {                              
      lv_label_set_text_fmt(lv_obj_get_child(tab_status, 0),
                          "A: %s\nB: %s",md->getModeName(ison?mode_a:M_NONE),
                          md->getModeName(ison?mode_b:M_NONE));
    }
    if (main_mode == MODE_MANUAL) {
      buttonbar->settext(2,"On\nOff");
      buttonbar->setvalue(2, ison ? 100: 0);
    } else {
      buttonbar->settext(2,"");
      buttonbar->setvalue(2,  0);
    }
  
    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      if (rand_timer->has_focus() || timer->has_focus())
        buttonbar->setvalue(4,  100);
      else
        buttonbar->setvalue(4,  0);
    }
  }
}

void tab_coyote::coyote_mode_change_cb(lv_event_t *event) {
  tab_coyote *ctab = static_cast<tab_coyote *>(lv_event_get_user_data(event));
  ctab->main_mode = static_cast<tab_coyote::main_modes>(lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(event)));
  ESP_LOGI("coyote", "cb %s on %d: new mode %d",
           pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(),
           ctab->main_mode);
  ctab->need_refresh = true;
  ctab->rand_timer->show((ctab->main_mode == tab_coyote::MODE_RANDOM));
  ctab->timer->show((ctab->main_mode == tab_coyote::MODE_TIMER));
  ctab->sync->show((ctab->main_mode == tab_coyote::MODE_SYNC));
}

void tab_coyote::tab_create_status(lv_obj_t *tv2) {
  tab_status = lv_obj_create(tv2);
  lv_obj_set_size(tab_status, 160-8-8, 96);
  lv_obj_align(tab_status, LV_ALIGN_TOP_LEFT, 8, 0);
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

void tab_coyote::coyote_tab_create() {
  page = lv_tabview_add_tab(tv, gettabname());

  lv_obj_set_style_pad_left(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_right(page,0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(page, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(page, 0, LV_PART_MAIN);

  modeselect->createdropdown(page, coyote_main_modes_c);
  lv_obj_add_event_cb(modeselect->getdropdownobject(), coyote_mode_change_cb, LV_EVENT_VALUE_CHANGED, this);
  
  buttonbar = new tab_object_buttonbar(page);

  tab_create_status(page);

  rand_timer->view(page);
  timer->view(page);
  sync->view(page);

  lv_tabview_set_act(tv, lv_get_tabview_idx_from_page(tv, page), LV_ANIM_OFF);
}

boolean tab_coyote::hardware_changed(void) {
  need_refresh = true;
  device_coyote2* cd = static_cast<device_coyote2*>(device);
  if (last_change == D_CONNECTING) {
    printf_log("Connecting %s\n", device->getShortName());
  } else if (last_change == D_CONNECTED) {
    coyote_tab_create();
    printf_log("Connected Coyote battery %d%%\n",cd->get().get_batterylevel());
    cd->get().chan_a().put_setmode(M_BREATH);
    cd->get().chan_b().put_setmode(M_BREATH);
    send_sync_data(SYNC_START);
    send_sync_data(SYNC_ON);
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", device->getShortName());
    send_sync_data(SYNC_BYE);
    return false;
  } 
  return true;
}