#include <memory>

#include "tab.hpp"
#include "lvgl-utils.h"
#include "tab-mqtt-socket.hpp"
#include "tab-mqtt.hpp"
#include "comms-wifi.hpp"

void send_mqtt_data(const char *topic, const char *message) {
  mqttsend(topic,message);
}

tab_mqtt_socket::tab_mqtt_socket(char *n, char *t) {
  strncpy(mqtt_topic, t, sizeof(mqtt_topic)-1);
  strncpy(mqtt_topic_name, n, sizeof(mqtt_topic_name)-1);
  ison = false;
  timer = new tab_object_timer(false);
  rand_timer = new tab_object_timer(true);
  sync = new tab_object_sync();
  modeselect = new tab_object_modes();
  modetimer = new tab_object_modetimer(timer, rand_timer, [this](bool active_on) {
      ison = active_on;
      if (active_on) {
          send_mqtt_data(mqtt_topic, "ON");
          send_sync_data(SYNC_ON);
      } else {
          send_mqtt_data(mqtt_topic, "OFF");
          send_sync_data(SYNC_OFF);
      }
  });
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
}
tab_mqtt_socket::~tab_mqtt_socket() {}


void tab_mqtt_socket::encoder_change(int sw, int change) {
  if (sw == tab_object_buttonbar::rotary4) {
    rand_timer->rotary_change(change);
    timer->rotary_change(change);
    modeselect->rotary_change(change);
  }
}

void tab_mqtt_socket::switch_change(int sw, bool value) {
  need_refresh = true;

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
    modetimer->set_is_on(false);
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

void tab_mqtt_socket::loop(bool activetab) {
  if (modetimer->update(main_mode == MODE_RANDOM || main_mode == MODE_TIMER, main_mode == MODE_RANDOM)) {
      need_refresh = true;
  }

  if (activetab && need_refresh) {
    ESP_LOGD("mqtt_socket", "refresh active tab from %s on %d",
             pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

    status->set_active(ison);

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = modetimer->get_remaining_seconds();
      status->set_text_fmt("%s\n%d", ison?"On":"Off", seconds);
    } else {
      status->set_text(ison?"On":"Off");
    }
    need_refresh = false;
    need_knob_refresh = true;
  }
  if (activetab && need_knob_refresh) {
    need_knob_refresh = false;

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

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      if (rand_timer->has_focus() || timer->has_focus())
        buttonbar->set_value(tab_object_buttonbar::rotary4,100);
      else
        buttonbar->set_value(tab_object_buttonbar::rotary4,0);
    }
  }
}


void tab_mqtt_socket::focus_change(bool focus) {
  ESP_LOGD("mqtt_socket", "focus cb %s on %d: %d",
           pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), focus);
  need_refresh = true;
  buttonbar->set_rgb_all(lv_color_hsv_to_rgb(0, 0, 0));
  //buttonbar->set_text(tab_object_buttonbar::rotary4, LV_SYMBOL_SETTINGS);
}


void tab_mqtt_socket::tab_create() {
  create_standard_page(mqtt_socket_main_modes_c);
  create_standard_widgets();

  mqttsubscribe(mqtt_topic);
}

// return false if we removed ourselves from the connected devices list
bool tab_mqtt_socket::hardware_changed(void) {
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