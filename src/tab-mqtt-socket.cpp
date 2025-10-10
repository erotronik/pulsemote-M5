#include <memory>

#include "tab.hpp"
#include "lvgl-utils.h"
#include "tab-mqtt-socket.hpp"
#include "tab-mqtt.hpp"
#include "comms-wifi.hpp"

tab_mqtt_socket::tab_mqtt_socket(char *n, char *t) {
  strncpy(mqtt_topic, t, sizeof(mqtt_topic)-1);
  strncpy(mqtt_topic_name, n, sizeof(mqtt_topic_name)-1);
  ison = false;
  main_mode = MODE_MANUAL;
  timer = new tab_object_timer(false);
  rand_timer = new tab_object_timer(true);
  sync = new tab_object_sync();
  modeselect = new tab_object_modes();
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
}
tab_mqtt_socket::~tab_mqtt_socket() {}

void send_mqtt_data(const char *topic, const char *message) {
  mqttsend(topic,message);
}

void tab_mqtt_socket::encoder_change(int sw, int change) {
  if (sw == tab_object_buttonbar::rotary1) {
    hue+=change*4;
    // send "wled/red/col" "#rrggbb"
    //lv_color_hsv_to_rgb(0, 100, power)
    //
    //char msg[8];
    lv_color_t rgb = lv_color_hsv_to_rgb(hue, 100, 50);
    //sprintf(msg,"#%02x%02x%02x",rgb.red,rgb.green,rgb.blue);
    buttonbar->set_rgb(tab_object_buttonbar::rotary1, rgb); 
    huesend = millis()+500;
    //send_mqtt_data("wled/red/col",msg);
  }
  if (sw == tab_object_buttonbar::rotary4) {
    rand_timer->rotary_change(change);
    timer->rotary_change(change);
    modeselect->rotary_change(change);
  }
}

void tab_mqtt_socket::switch_change(int sw, boolean value) {
  need_refresh = true;

  if (sw == tab_object_buttonbar::rotary1 && value) {
    preset++;
    if (preset > 5) preset = 0;
    char msg[16];
    sprintf(msg,"{\\\"ps\\\":%d}",preset); 
    send_mqtt_data("wled/red/api",msg);
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
      ison = 1;
      send_mqtt_data(mqtt_topic, "ON");
      send_sync_data(SYNC_ON);
    } else if (sw == tab_object_buttonbar::switch1 && value && ison == 1) {
      ison = 0;
      send_mqtt_data(mqtt_topic, "OFF");
      send_sync_data(SYNC_OFF);
    }
  }

  if (main_mode != MODE_MANUAL && sw == tab_object_buttonbar::switch1 && value) {  // Stop
    main_mode = MODE_MANUAL;
    modeselect->reset();
    send_mqtt_data(mqtt_topic, "OFF");
    ison = false;
  }
}

// another device can push data to us when they connect, disconnect, turn on, turn off

void tab_mqtt_socket::gotsyncdata(Tab *t, sync_data syncstatus) {
  ESP_LOGD("mqtt_socket", "got sync data %d from %s", syncstatus, t->gettabname());
  if (syncstatus == SYNC_ALLOFF) {
    main_mode = MODE_MANUAL;
    modeselect->reset();
    ison = false;
    send_mqtt_data(mqtt_topic, "OFF");
    need_refresh = true;
  }
  // if we sync on wifi then we'll get in a loop
  if (main_mode == MODE_SYNC && strncmp(t->gettabname(),"wifi",4)) {
    bool isinverted = sync->isinverted();
    if ((syncstatus == SYNC_ON && !isinverted) || (syncstatus == SYNC_OFF && isinverted)) {
      ison = true;
      send_mqtt_data(mqtt_topic, "ON");
    } else if ((syncstatus == SYNC_OFF && !isinverted) || (syncstatus == SYNC_ON && isinverted)) {
      ison = false;
      send_mqtt_data(mqtt_topic, "OFF");
    }
    need_refresh = true;
  }
}

void tab_mqtt_socket::loop(boolean activetab) {

  if (huesend !=0 && millis() > huesend) {
    huesend = 0;
    char msg[8];
    lv_color_t rgb = lv_color_hsv_to_rgb(hue, 100, 100);
    sprintf(msg,"#%02x%02x%02x",rgb.red,rgb.green,rgb.blue);
    send_mqtt_data("wled/red/col",msg);
  }

  if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
    if (timermillis < millis()) {
      need_refresh = true;
      if (ison == 0) {
        ison = 1;
        send_sync_data(SYNC_ON);
        send_mqtt_data(mqtt_topic, "ON");
        if (main_mode == MODE_RANDOM)
          timermillis = millis() + rand_timer->gettimeon() * 1000;
        else
          timermillis = millis() + timer->gettimeon() * 1000;
      } else {
        ison = 0;
        send_sync_data(SYNC_OFF);
        send_mqtt_data(mqtt_topic, "OFF");
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
    ESP_LOGD("mqtt_socket", "refresh active tab from %s on %d",
             pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

    lv_obj_set_style_bg_color(tab_status, lv_color_hex(ison?COLOUR_GREEN:COLOUR_RED), LV_PART_MAIN);

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = (timermillis - millis()) / 1000;
      lv_label_set_text_fmt(lv_obj_get_child(tab_status, 0), "%s\n%d", ison?"On":"Off", seconds);
    } else {
      lv_label_set_text(lv_obj_get_child(tab_status, 0), ison?"On":"Off");
    }
    need_refresh = false;
    need_knob_refresh = true;
  }
  if (activetab && need_knob_refresh) {
    need_knob_refresh = false;

    buttonbar->set_text(tab_object_buttonbar::rotary1,"LED");
    buttonbar->set_rgb(tab_object_buttonbar::rotary1, lv_color_hsv_to_rgb(hue, 100, 50));
    if (main_mode == MODE_MANUAL) {
      buttonbar->set_text(tab_object_buttonbar::switch1,"On\nOff");
      buttonbar->set_value(tab_object_buttonbar::switch1,ison? 100:0);
    } else {
      buttonbar->set_text(tab_object_buttonbar::switch1,"Stop");
      buttonbar->set_value(tab_object_buttonbar::switch1,0);
    }

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      if (rand_timer->has_focus() || timer->has_focus())
        buttonbar->set_value(tab_object_buttonbar::rotary4,100);
      else
        buttonbar->set_value(tab_object_buttonbar::rotary4,0);
    }

    //for (int i : {tab_object_buttonbar::rotary1, tab_object_buttonbar::rotary2, tab_object_buttonbar::rotary3, tab_object_buttonbar::rotary4, tab_object_buttonbar::switch1})
    //  buttonbar->set_text(i,"");
    //if (main_mode == MODE_MANUAL) {
    //  buttonbar->set_text_fmt(tab_object_buttonbar::switch1,"On\nOff");
    //  buttonbar->set_value(tab_object_buttonbar::switch1,ison? 100:0);
    //} else
    //  buttonbar->set_value(tab_object_buttonbar::switch1,0);

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      if (rand_timer->has_focus() || timer->has_focus())
        buttonbar->set_value(tab_object_buttonbar::rotary4,100);
      else
        buttonbar->set_value(tab_object_buttonbar::rotary4,0);
    }
  }
}

void mqtt_socket_mode_change_cb(lv_event_t *event) {
  tab_mqtt_socket *mqtt_socket_tab =
      static_cast<tab_mqtt_socket *>(lv_event_get_user_data(event));
  mqtt_socket_tab->main_mode = static_cast<tab_mqtt_socket::main_modes>(
      lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(event)));
  ESP_LOGI("mqtt_socket", "cb %s on %d: new mode %d",
           pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(),
           mqtt_socket_tab->main_mode);
  mqtt_socket_tab->need_refresh = true;
  mqtt_socket_tab->rand_timer->show((mqtt_socket_tab->main_mode == tab_mqtt_socket::MODE_RANDOM));
  mqtt_socket_tab->timer->show((mqtt_socket_tab->main_mode == tab_mqtt_socket::MODE_TIMER));
  mqtt_socket_tab->sync->show((mqtt_socket_tab->main_mode == tab_mqtt_socket::MODE_SYNC));
}

void tab_mqtt_socket::focus_change(boolean focus) {
  ESP_LOGD("mqtt_socket", "focus cb %s on %d: %d",
           pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), focus);
  need_refresh = true;
  buttonbar->set_rgb_all(lv_color_hsv_to_rgb(0, 0, 0));
  buttonbar->set_text(tab_object_buttonbar::rotary4, LV_SYMBOL_SETTINGS);
}

void tab_mqtt_socket::tab_create_status(lv_obj_t *tv2) {
  tab_status = lv_obj_create(tv2);

  lv_obj_add_style(tab_status, &lvpulsemote_style_status, LV_PART_MAIN);
  lv_obj_set_size(tab_status, 108, 64);
  lv_obj_align(tab_status, LV_ALIGN_TOP_LEFT, 4, 0);
  lv_obj_set_scrollbar_mode(tab_status, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *labelx = lv_label_create(tab_status);
  lv_label_set_text(labelx, "-");
  lv_obj_align(labelx, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *extra_label = lv_label_create(tab_status);
  lv_label_set_text(extra_label, "");
  lv_obj_align(extra_label, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void tab_mqtt_socket::tab_create() {
  page = lv_tabview_add_tab(tv, gettabname());
  lv_obj_add_style(page, &lvpulsemote_style_tab, LV_PART_MAIN);

  modeselect->createdropdown(page, mqtt_socket_main_modes_c);
  lv_obj_add_event_cb(modeselect->getdropdownobject(), mqtt_socket_mode_change_cb, LV_EVENT_VALUE_CHANGED, this);

  buttonbar = new tab_object_buttonbar(page);
  tab_create_status(page);
  rand_timer->view(page);
  timer->view(page);
  sync->view(page);
  mqttsubscribe(mqtt_topic);

  lv_tabview_set_act(tv, lv_get_tabview_idx_from_page(tv, page), LV_ANIM_OFF);
}

// return false if we removed ourselves from the connected devices list
boolean tab_mqtt_socket::hardware_changed(void) {
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
    send_sync_data(SYNC_BYE);
    return false;
  }
  return true;
}