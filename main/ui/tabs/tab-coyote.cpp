#include <memory>
#include "device-coyote.hpp"
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
    status = nullptr;
    device = nullptr;
    ison = true;
    level_a_req = 0;
    level_b_req = 0;
}
tab_coyote::~tab_coyote() {}

void tab_coyote::gotsyncdata(Tab *t, sync_data syncstatus) {
  auto md = static_cast<device_coyote*>(device);
  if (!md) return;
  ESP_LOGD("coyote", "got sync data %d from %s", syncstatus, t->gettabname());
  if (syncstatus == SYNC_ALLOFF) {
    main_mode = MODE_MANUAL;
    modeselect->reset();
    ison = false;
    md->set_ab_mode(M_NONE,M_NONE);
  }
  if (main_mode == MODE_SYNC) {
    bool isinverted = sync->isinverted();
    if ((syncstatus == SYNC_ON && !isinverted) || (syncstatus == SYNC_OFF && isinverted)) {
      ison = true;
      md->set_ab_mode(mode_a,mode_b);
    } else if ((syncstatus == SYNC_OFF && !isinverted) || (syncstatus == SYNC_ON && isinverted)) {
      ison = false;
      md->set_ab_mode(M_NONE,M_NONE);
    }
    need_refresh = true;
  }
}

void tab_coyote::switch_change(int sw, bool state) {
  need_refresh = true;

  if (sw == tab_object_buttonbar::rotary4 && state) {
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

  if (main_mode == MODE_MANUAL && sw == tab_object_buttonbar::switch1 && state) {
    auto md = static_cast<device_coyote*>(device);
    if (ison == 0) {
      ison = 1;
      md->set_ab_mode(mode_a,mode_b);
      send_sync_data(SYNC_ON);
    } else {
      ison = 0;
      md->set_ab_mode(M_NONE,M_NONE);
      send_sync_data(SYNC_OFF);
    }
  }

  if (main_mode != MODE_MANUAL && sw == tab_object_buttonbar::switch1 && state) {  // Stop
    auto md = static_cast<device_coyote*>(device);   
    main_mode = MODE_MANUAL;
    modeselect->reset();
    ison = false;
    md->set_ab_mode(M_NONE,M_NONE);
  }

  if (sw == tab_object_buttonbar::rotary1 || sw == tab_object_buttonbar::rotary2) { // click to move to the next mode, then back to start
    auto md = static_cast<device_coyote*>(device);
    coyote_mode mode;
    if (sw == tab_object_buttonbar::rotary1) 
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
    if (sw == tab_object_buttonbar::rotary1)  {
      mode_a = md->modes[i];
      md->set_ab_mode(mode_a,-1);
    } else {
      mode_b = md->modes[i];
      md->set_ab_mode(-1,mode_b);
    }
  }
}

void tab_coyote::encoder_change(int sw, int change) {
  device_coyote *md = static_cast<device_coyote*>(device);
  need_refresh = true;

  if (sw == tab_object_buttonbar::rotary1) {
    level_a_req = std::min(99, std::max(0, level_a_req + change));
    md->get().chan_a().put_power_pc(level_a_req);
  }
  else if (sw == tab_object_buttonbar::rotary2) {
    level_b_req = std::min(99, std::max(0, level_b_req + change));
    md->get().chan_b().put_power_pc(level_b_req); 
  }
  else if (sw == tab_object_buttonbar::rotary4) {
    rand_timer->rotary_change(change);
    timer->rotary_change(change);
    modeselect->rotary_change(change);
  }
}

void tab_coyote::focus_change(bool focus) {
  buttonbar->set_rgb_all(lv_color_hsv_to_rgb(0, 0, 0));
  //buttonbar->set_text(tab_object_buttonbar::rotary4, LV_SYMBOL_SETTINGS);
  need_refresh = true;
}

void tab_coyote::loop(bool active) {
  auto md = static_cast<device_coyote*>(device);
  if (!md) return;
  //if (!md->is_connected) return;
  // ESP_LOGE("coyote", "loop");

  if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {

    if (timermillis < millis()) {
      auto md = static_cast<device_coyote*>(device);
      need_refresh = true;
      if (ison == 0) {
        ison = 1;
        md->set_ab_mode(mode_a,mode_b);
        send_sync_data(SYNC_ON);
        timermillis = millis() + (main_mode == MODE_RANDOM ? rand_timer->gettimeon() : timer->gettimeon()) * 1000;
      } else {
        ison = 0;
        md->set_ab_mode(M_NONE,M_NONE);
        send_sync_data(SYNC_OFF);
        timermillis = millis() + (main_mode == MODE_RANDOM ? rand_timer->gettimeoff() : timer->gettimeon()) * 1000;
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
  if (need_refresh) last_refresh = millis();

  // check the power level 500mS after we've done something, just to make sure
  if (last_refresh !=0 && last_refresh+500 < millis()) {
    auto md = static_cast<device_coyote*>(device);
    last_refresh = 0;
    need_refresh = true;
    level_a_req = md->get().chan_a().get_power_pc();
    level_b_req = md->get().chan_b().get_power_pc();
    ESP_LOGD("coyote","did a refresh to A %d B %d", level_a_req, level_b_req);
  }

  if (need_refresh && buttonbar) {
    auto md = static_cast<device_coyote*>(device);
    need_refresh = false;

    //int power = md->get().chan_a().get_power_pc();
    buttonbar->set_value(tab_object_buttonbar::rotary1, level_a_req); 
    buttonbar->set_ison(tab_object_buttonbar::rotary1, ison);
    buttonbar->set_text_fmt(tab_object_buttonbar::rotary1, "A\n%d%%", level_a_req);
    if (active) buttonbar->set_rgb(tab_object_buttonbar::rotary1, lv_color_hsv_to_rgb(0, 100, level_a_req));

    //power = md->get().chan_b().get_power_pc();
    buttonbar->set_value(tab_object_buttonbar::rotary2, level_b_req); 
    buttonbar->set_ison(tab_object_buttonbar::rotary2, ison);
    buttonbar->set_text_fmt(tab_object_buttonbar::rotary2, "B\n%d%%", level_b_req);
    if (active) buttonbar->set_rgb(tab_object_buttonbar::rotary2, lv_color_hsv_to_rgb(0, 100, level_b_req));

    buttonbar->set_click_text(tab_object_buttonbar::rotary1,ison?"Mode A":"");
    buttonbar->set_click_text(tab_object_buttonbar::rotary2,ison?"Mode B":"");

    if (ison) {
      mode_a = md->get().chan_a().get_mode();
      mode_b = md->get().chan_b().get_mode();
    } 
    status->set_active(ison);
    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = (timermillis - millis()) / 1000;
      status->set_text_fmt("A: %s\nB: %s\n%d", md->getModeName(ison?mode_a:M_NONE), md->getModeName(ison?mode_b:M_NONE), seconds);
    } else {                              
      status->set_text_fmt("A: %s\nB: %s",md->getModeName(ison?mode_a:M_NONE), md->getModeName(ison?mode_b:M_NONE));
    }
    if (main_mode == MODE_MANUAL) {
      buttonbar->set_text(tab_object_buttonbar::switch1,"On\nOff");
      buttonbar->set_value(tab_object_buttonbar::switch1, ison ? 100: 0);
    } else {
      buttonbar->set_text(tab_object_buttonbar::switch1,"Stop");
      buttonbar->set_value(tab_object_buttonbar::switch1,  0);
    }
  
    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      if (rand_timer->has_focus() || timer->has_focus())
        buttonbar->set_value(tab_object_buttonbar::rotary4,  100);
      else
        buttonbar->set_value(tab_object_buttonbar::rotary4,  0);
    }
  }


}

void tab_coyote::coyote_mode_change_cb(lv_event_t *event) {
  auto ctab = static_cast<tab_coyote *>(lv_event_get_user_data(event));
  ctab->main_mode = static_cast<tab_coyote::main_modes>(lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(event)));
  ESP_LOGI("coyote", "cb %s on %d: new mode %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), ctab->main_mode);
  ctab->need_refresh = true;
  ctab->rand_timer->show((ctab->main_mode == tab_coyote::MODE_RANDOM));
  ctab->timer->show((ctab->main_mode == tab_coyote::MODE_TIMER));
  ctab->sync->show((ctab->main_mode == tab_coyote::MODE_SYNC));
}


void tab_coyote::coyote_tab_create() {
  page = lv_tabview_add_tab(tv, gettabname());
  lv_obj_add_style(page, &lvpulsemote_style_tab, LV_PART_MAIN);

  modeselect->createdropdown(page, coyote_main_modes_c);
  lv_obj_add_event_cb(modeselect->getdropdownobject(), coyote_mode_change_cb, LV_EVENT_VALUE_CHANGED, this);
  
  buttonbar = new tab_object_buttonbar(page);
  buttonbar->set_onmain(tab_object_buttonbar::rotary1, true);
  buttonbar->set_onmain(tab_object_buttonbar::rotary2, true);

  status = new tab_object_status(page, 160-8-8, 96);
  status->align(LV_ALIGN_TOP_LEFT, LV_SCALE(8), 0);

  rand_timer->view(page);
  timer->view(page);
  sync->view(page);

  buttonbar->set_click_text(tab_object_buttonbar::rotary4, LV_SYMBOL_SETTINGS);
  lv_tabview_set_act(tv, lv_get_tabview_idx_from_page(tv, page), LV_ANIM_OFF);
}

bool tab_coyote::hardware_changed(void) {
  need_refresh = true;
  if (last_change == D_CONNECTING) {
    printf_log("Connecting %s\n", device->getShortName());
  } else if (last_change == D_CONNECTED) {
    device_coyote* cd = static_cast<device_coyote*>(device);
    coyote_tab_create();
    printf_log("Connected %s battery %d%%\n",device->getShortName(),cd->get().get_batterylevel());
    send_sync_data(SYNC_START);
    cd->set_ab_mode(M_BREATH,M_BREATH);
    send_sync_data(SYNC_ON);
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", device->getShortName());
    send_sync_data(SYNC_BYE);
    return false;
  } 
  return true;
}