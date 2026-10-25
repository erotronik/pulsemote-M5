#include <memory>
#include <ArduinoJson.h>

#include "tab.hpp"
#include "lvgl-utils.h"
#include "tab-mqtt-socket.hpp"
#include "tab-mqtt.hpp"
#include "comms-wifi.hpp"

tab_mqtt_socket::tab_mqtt_socket(char *n, char *t) {
  strncpy(mqtt_topic, t, sizeof(mqtt_topic)-1);
  strncpy(mqtt_topic_name, n, sizeof(mqtt_topic_name)-1);
  strncpy(mqtt_state_topic, t, sizeof(mqtt_state_topic)-1);
  char *set = strrchr(mqtt_state_topic, '/');
  if (set && !strcmp(set, "/set")) *set = '\0';
  ison = false;
  timer = new tab_object_timer(false);
  rand_timer = new tab_object_timer(true);
  sync = new tab_object_sync();
  modeselect = new tab_object_modes();
  modetimer = new tab_object_modetimer(timer, rand_timer, [this](bool active_on) {
      ison = active_on;
      if (active_on) {
          send_state(true);
          send_sync_data(SYNC_ON);
      } else {
          send_state(false);
          send_sync_data(SYNC_OFF);
      }
  });
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
}
tab_mqtt_socket::~tab_mqtt_socket() {}

void tab_mqtt_socket::send_state(bool on) {
  mqttsend(mqtt_topic, on ? "ON" : "OFF");
  wanted = on;
  waiting_since = millis();
  no_reply = false;
  need_refresh = true;
}


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
      send_state(true);
      send_sync_data(SYNC_ON);
    } else if (sw == tab_object_buttonbar::switch1 && value && ison == 1) {
      ison = 0;
      send_state(false);
      send_sync_data(SYNC_OFF);
    }
  }

  if (main_mode != MODE_MANUAL && sw == tab_object_buttonbar::switch1 && value) {  // Stop
    main_mode = MODE_MANUAL;
    modetimer->set_is_on(false);
    modeselect->reset();
    send_state(false);
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
    send_state(false);
    need_refresh = true;
  }
  // if we sync on wifi then we'll get in a loop
  if (main_mode == MODE_SYNC && strncmp(t->gettabname(),"wifi",4)) {
    bool isinverted = sync->isinverted();
    if ((syncstatus == SYNC_ON && !isinverted) || (syncstatus == SYNC_OFF && isinverted)) {
      ison = true;
      send_state(true);
    } else if ((syncstatus == SYNC_OFF && !isinverted) || (syncstatus == SYNC_ON && isinverted) ||
               syncstatus == SYNC_BYE) {  // the source has gone away
      ison = false;
      send_state(false);
    }
    need_refresh = true;
  }
}

void tab_mqtt_socket::loop(bool activetab) {
  // Zigbee2MQTT gives up on a socket that's lost its power after about 10s
  if (waiting_since && millis() - waiting_since > 12000) {
    waiting_since = 0;
    no_reply = true;
    if (main_mode == MODE_MANUAL) ison = reported == 1;
    need_refresh = true;
  }

  if (modetimer->update(main_mode == MODE_RANDOM || main_mode == MODE_TIMER, main_mode == MODE_RANDOM)) {
      need_refresh = true;
  }

  if (activetab && need_refresh) {
    ESP_LOGD("mqtt_socket", "refresh active tab from %s on %d",
             pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

    status->set_active(ison);

    const char *state = waiting_since ? (wanted ? "On..." : "Off...") : no_reply ? "No reply" : ison ? "On" : "Off";
    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = modetimer->get_remaining_seconds();
      status->set_text_fmt("%s\n%d", state, seconds);
    } else {
      status->set_text(state);
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

  mqttsubscribe(mqtt_state_topic);
}

// Zigbee2MQTT's state for the socket, like {"state":"ON","linkquality":42}, whenever it changes,
// whoever changed it (us, Home Assistant, the socket's own button)
void tab_mqtt_socket::mqtt_received(const char *topic, const char *message) {
  if (strcmp(topic, mqtt_state_topic)) return;
  JsonDocument doc;
  if (deserializeJson(doc, message)) return;
  const char *state = doc["state"] | "";
  if (!*state) return;
  reported = !strcmp(state, "ON");
  if (waiting_since && reported == wanted) waiting_since = 0;
  no_reply = false;
  // In Manual it's whatever the socket is, unless we're still waiting to hear it's changed (other
  // updates, like the signal strength, still have the old state); the other modes decide for
  // themselves
  if (main_mode == MODE_MANUAL && !waiting_since) ison = reported;
  need_refresh = true;
}

// return false if we removed ourselves from the connected devices list
bool tab_mqtt_socket::hardware_changed(void) {
  need_refresh = true;
  if (last_change == D_CONNECTING) {
    printf_log("Connecting %s\n", gettabname());
  } else if (last_change == D_CONNECTED) {
    tab_create();
    send_sync_data(SYNC_START);
    send_sync_data(SYNC_ON);
    printf_log("Connected %s\n", gettabname());
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", gettabname());
    send_sync_data(SYNC_BYE);
    return false;
  }
  return true;
}