#include <memory>
#include "device-coyote2.hpp"
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include "tab.hpp"
#include "tab-coyote.hpp"
#include "lvgl-utils.h"

const char *coyote_main_modes_c =
    "Manual\nTimer\nRandom\nSync";  

tab_coyote::tab_coyote() {
    page = nullptr;
    old_last_change = last_change = D_NONE;
    main_mode = MODE_MANUAL;
    timer = new tab_object_timer(false);
    rand_timer = new tab_object_timer(true);
    sync = new tab_object_sync();
    device = nullptr;
    bool need_refresh =false;
    ison = true;
}
tab_coyote::~tab_coyote() {}

void tab_coyote::gotsyncdata(Tab *t, sync_data syncstatus) {
  device_coyote2 *md = static_cast<device_coyote2*>(device);
  if (!md) return;
  ESP_LOGD("coyote", "got sync data %d from %s\n", syncstatus, t->gettabname());
  if (main_mode == MODE_SYNC) {
    bool isinverted = sync->isinverted();
    if ((syncstatus == SYNC_ON && !isinverted) || (syncstatus == SYNC_OFF && isinverted)) {
      ison = true;
      ison = 1;
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

  if (main_mode == MODE_RANDOM && sw == 3 && state) {
    rand_timer->highlight_next_field();
  }
  if (main_mode == MODE_TIMER && sw == 3 && state) {
    timer->highlight_next_field();
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
    if (sw ==1) 
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
      i = i+1;
    }
    if (sw ==1)  {
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
  if (sw==1) 
    md->get().chan_a().put_power_diff(change);
  else if (sw==0)
    md->get().chan_b().put_power_diff(change); 
  if (main_mode == MODE_RANDOM && sw == 3)
    rand_timer->rotary_change(change);
  if (main_mode == MODE_TIMER && sw == 3) 
    timer->rotary_change(change);
}

void tab_coyote::focus_change(boolean focus) {
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
    buttonbar->setrgb(0, hsvToRgb(0, 255, power * 2 + 5));

    power = md->get().chan_b().get_power_pc();
    buttonbar->setvalue(1, power); 
    buttonbar->settextfmt(1, "B\n%" LV_PRId32 "%%", power);
    buttonbar->setrgb(1, hsvToRgb(0, 255, power *2 + 5));

    if (ison) {
      mode_a = md->get().chan_a().get_mode();
      mode_b = md->get().chan_b().get_mode();
    } 
    lv_obj_set_style_bg_color(tab_status, lv_color_hex(ison?COLOUR_GREEN:COLOUR_RED),
                                LV_PART_MAIN);

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = (timermillis - millis()) / 1000;
      lv_label_set_text_fmt(lv_obj_get_child(tab_status, 0), "%s\n%s\n%d",
                              md->getModeName(ison?mode_a:M_NONE),
                              md->getModeName(ison?mode_b:M_NONE),
                              seconds);
    } else {                              
      lv_label_set_text_fmt(lv_obj_get_child(tab_status, 0),
                          "%s\n%s",md->getModeName(ison?mode_a:M_NONE),
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
      buttonbar->settext(4, LV_SYMBOL_BELL);
      if (main_mode == MODE_RANDOM && rand_timer->has_active_button() ||
          main_mode == MODE_TIMER && timer->has_active_button())
        buttonbar->setvalue(4,  100);
      else
        buttonbar->setvalue(4,  0);
    } else 
      buttonbar->settext(4, "");
  }
}

void coyote_mode_change_cb(lv_event_t *event) {
  tab_coyote *ctab =
      static_cast<tab_coyote *>(lv_event_get_user_data(event));
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
  lv_obj_t *square = lv_obj_create(tv2);
  lv_obj_set_size(square, 108, 96);
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

void tab_coyote::coyote_tab_create() {
  device_coyote2* md = static_cast<device_coyote2*>(device);
  lv_obj_t *tv1 = lv_tabview_add_tab(tv, md->getShortName());

  lv_obj_set_style_pad_left(tv1, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_right(tv1,0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(tv1, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(tv1, 0, LV_PART_MAIN);

  lv_obj_t *dd = lv_dropdown_create(tv1);
  lv_obj_set_style_text_font(dd, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_align(dd, LV_ALIGN_TOP_RIGHT);
  lv_obj_set_size(dd, 160, 36);  // match the timer box width

  lv_dropdown_set_options(dd, coyote_main_modes_c);
  lv_obj_add_event_cb(dd, coyote_mode_change_cb, LV_EVENT_VALUE_CHANGED, this);
  
  buttonbar = new tab_object_buttonbar(tv1);

  page = tv1;
  int tabid = lv_get_tabview_idx_from_page(tv, tv1);
  tab_create_status(tv1);

  lv_obj_t *lt = rand_timer->view(tv1);
  lv_obj_align(lt, LV_ALIGN_TOP_RIGHT, 0, 40);
  rand_timer->show(false);

  lt = timer->view(tv1);
  lv_obj_align(lt, LV_ALIGN_TOP_RIGHT, 0, 40);
  timer->show(false);

  lt = sync->view(tv1);
  lv_obj_align(lt, LV_ALIGN_TOP_RIGHT, 0, 40);
  sync->show(false);

  lv_tabview_set_act(tv,tabid, LV_ANIM_OFF);
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