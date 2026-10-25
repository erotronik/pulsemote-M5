#include <algorithm>
#include <memory>

#include "tab.hpp"
#include "lvgl-utils.h"
#include "tab-mqtt-leds.hpp"
#include "tab-mqtt.hpp"
#include "comms-wifi.hpp"

void send_led_mqtt_data(const char *topic, const char *topic2, const char *message) {
  static char mqtt_topic_append[100];
  strncpy(mqtt_topic_append, topic, 99);
  strncat(mqtt_topic_append, topic2, 99);
  mqttsend(mqtt_topic_append,message);
}

void send_led_onoff(const char *mqtt_topic, bool on) {
  // A boolean: WLED takes a string starting with 't' as toggle
  send_led_mqtt_data(mqtt_topic, "/api", on?"{\"on\":true}":"{\"on\":false}");
}

tab_mqtt_leds::tab_mqtt_leds(char *n, char *t) {
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
          send_led_onoff(mqtt_topic, true);
          send_sync_data(SYNC_ON);
      } else {
          send_led_onoff(mqtt_topic, false);
          send_sync_data(SYNC_OFF);
      }
  });
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
  huesend = 0;
}
tab_mqtt_leds::~tab_mqtt_leds() {}


void tab_mqtt_leds::send_preset(void) {
  char msg[16];
  snprintf(msg, sizeof(msg), "{\"ps\":%d}", preset);
  send_led_mqtt_data(mqtt_topic, "/api", msg);
}

void tab_mqtt_leds::encoder_change(int sw, int change) {
  if (sw == tab_object_buttonbar::rotary1 && ison) {
    hue = (hue + change*4 + 360) % 360;
    if (huesend == 0) huesend = millis() + 250;
    last_turned = millis();
    need_knob_refresh = true;
  }
  if (sw == tab_object_buttonbar::rotary2 && ison) {
    preset = (preset - 1 + change % presets_max + presets_max) % presets_max + 1;
    send_preset();
    need_refresh = true;
  }
  if (sw == tab_object_buttonbar::rotary3 && ison) {
    brightness = std::min(255, std::max(1, brightness + change * 8));
    if (brightnesssend == 0) brightnesssend = millis() + 250;
    last_turned = millis();
    need_knob_refresh = true;
  }

  if (sw == tab_object_buttonbar::rotary4) {
    rand_timer->rotary_change(change);
    timer->rotary_change(change);
    modeselect->rotary_change(change);
  }
}

void tab_mqtt_leds::switch_change(int sw, bool value) {
  need_refresh = true;

  if (sw == tab_object_buttonbar::rotary2 && value && ison) {
    preset = preset % presets_max + 1;
    send_preset();
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
      send_led_onoff(mqtt_topic,true);
      send_sync_data(SYNC_ON);
    } else if (sw == tab_object_buttonbar::switch1 && value && ison == 1) {
      ison = 0;
      send_led_onoff(mqtt_topic,false);
      //send_mqtt_data(mqtt_topic, "OFF");
      send_sync_data(SYNC_OFF);
    }
  }

  if (main_mode != MODE_MANUAL && sw == tab_object_buttonbar::switch1 && value) {  // Stop
    main_mode = MODE_MANUAL;
    modetimer->set_is_on(false);
    modeselect->reset();
    send_led_onoff(mqtt_topic,false);
    ison = false;
  }
}

// another device can push data to us when they connect, disconnect, turn on, turn off

void tab_mqtt_leds::gotsyncdata(Tab *t, sync_data syncstatus) {
  ESP_LOGD("mqtt_leds", "got sync data %d from %s", syncstatus, t->gettabname());
  if (syncstatus == SYNC_ALLOFF) {
    main_mode = MODE_MANUAL;
    modeselect->reset();
    ison = false;
    send_led_onoff(mqtt_topic,ison);
    //send_mqtt_data(mqtt_topic, "OFF");
    need_refresh = true;
  }
  // if we sync on wifi then we'll get in a loop
  if (main_mode == MODE_SYNC && strncmp(t->gettabname(),"wifi",4)) {
    bool isinverted = sync->isinverted();
    if ((syncstatus == SYNC_ON && !isinverted) || (syncstatus == SYNC_OFF && isinverted)) {
      ison = true;
      send_led_onoff(mqtt_topic,ison);
      //send_mqtt_data(mqtt_topic, "ON");
    } else if ((syncstatus == SYNC_OFF && !isinverted) || (syncstatus == SYNC_ON && isinverted) ||
               syncstatus == SYNC_BYE) {  // the source has gone away
      ison = false;
      send_led_onoff(mqtt_topic,ison);
      //send_mqtt_data(mqtt_topic, "OFF");
    }
    need_refresh = true;
  }
}

void tab_mqtt_leds::loop(bool activetab) {

  if (huesend !=0 && millis() > huesend) {
    huesend = 0;
    char msg[8];
    lv_color_t rgb = lv_color_hsv_to_rgb(hue, 100, 100);
    sprintf(msg,"#%02x%02x%02x",rgb.red,rgb.green,rgb.blue);
    send_led_mqtt_data(mqtt_topic,"/col",msg);
  }
  if (brightnesssend != 0 && millis() > brightnesssend) {
    brightnesssend = 0;
    char msg[16];
    snprintf(msg, sizeof(msg), "{\"bri\":%d}", brightness);
    send_led_mqtt_data(mqtt_topic, "/api", msg);
  }

  if (modetimer->update(main_mode == MODE_RANDOM || main_mode == MODE_TIMER, main_mode == MODE_RANDOM)) {
      need_refresh = true;
  }

  if (activetab && need_refresh) {
    ESP_LOGD("mqtt_leds", "refresh active tab from %s on %d",
             pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

    status->set_active(ison);

    if (main_mode == MODE_RANDOM || main_mode == MODE_TIMER) {
      int seconds = modetimer->get_remaining_seconds();
      status->set_text_fmt("%s %d%%\n%d", ison?"On":"Off", brightness * 100 / 255, seconds);
    } else if (preset) {
      status->set_text_fmt("%s %d%%\nPreset %d", ison?"On":"Off", brightness * 100 / 255, preset);
    } else {
      status->set_text_fmt("%s %d%%", ison?"On":"Off", brightness * 100 / 255);
    }
    need_refresh = false;
    need_knob_refresh = true;
  }
  if (activetab && need_knob_refresh) {
    need_knob_refresh = false;

    buttonbar->set_text(tab_object_buttonbar::rotary1, ison ? "Colour" : "");
    buttonbar->set_value(tab_object_buttonbar::rotary1, ison ? hue * 100 / 359 : 0);
    if (preset)
      buttonbar->set_text_fmt(tab_object_buttonbar::rotary2, ison ? "Preset\n%d" : "", preset);
    else
      buttonbar->set_text(tab_object_buttonbar::rotary2, ison ? "Preset" : "");
    buttonbar->set_value(tab_object_buttonbar::rotary2, ison ? preset * 100 / presets_max : 0);
    buttonbar->set_click_text(tab_object_buttonbar::rotary2, ison ? "next" : "");
    if (ison) {
      buttonbar->set_level(tab_object_buttonbar::rotary3, "Bright", brightness * 100 / 255);
    } else {
      buttonbar->clear_level(tab_object_buttonbar::rotary3);
      buttonbar->set_text(tab_object_buttonbar::rotary3, "");
      buttonbar->set_value(tab_object_buttonbar::rotary3, 0);
    }
    buttonbar->set_rgb(tab_object_buttonbar::rotary1, ison?lv_color_hsv_to_rgb(hue, 100, 50):lv_color_hex(0));
    buttonbar->set_rgb(tab_object_buttonbar::rotary2, ison?lv_color_hsv_to_rgb(0, 0, 20):lv_color_hex(0));
    buttonbar->set_rgb(tab_object_buttonbar::rotary3, ison?lv_color_hsv_to_rgb(0, 0, brightness * 50 / 255):lv_color_hex(0));

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


void tab_mqtt_leds::focus_change(bool focus) {
  ESP_LOGD("mqtt_leds", "focus cb %s on %d: %d",
           pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), focus);
  need_refresh = true;
  buttonbar->set_rgb_all(lv_color_hsv_to_rgb(0, 0, 0));
  buttonbar->set_text(tab_object_buttonbar::rotary4, LV_SYMBOL_SETTINGS);
}


void tab_mqtt_leds::tab_create() {
  create_standard_page(mqtt_leds_main_modes_c);
  create_standard_widgets();

  // What WLED says about itself when anything changes it: brightness (0 when off), colour, and
  // its state as XML, which has the preset
  for (const char *sub : {"/g", "/c", "/v"}) {
    char topic[max_topic_size];
    snprintf(topic, sizeof(topic), "%s%s", mqtt_topic, sub);
    mqttsubscribe(topic);
  }
}

void tab_mqtt_leds::mqtt_received(const char *topic, const char *message) {
  size_t len = strlen(mqtt_topic);
  if (strncmp(topic, mqtt_topic, len) || topic[len] != '/') return;
  const char *sub = topic + len + 1;
  // Not while a knob is being turned, when these are still catching up
  bool turning = millis() - last_turned < 1500;

  if (!strcmp(sub, "g")) {
    int g = atoi(message);
    if (!turning && g > 0) brightness = g;
    // In Manual it's whatever WLED says, like when it's turned on from its own app; the other modes
    // decide for themselves
    if (main_mode == MODE_MANUAL) ison = g > 0;
  } else if (!strcmp(sub, "c") && !turning && message[0] == '#') {
    unsigned long rgb = strtoul(message + 1, nullptr, 16);
    lv_color_hsv_t hsv = lv_color_to_hsv(lv_color_make(rgb >> 16, (rgb >> 8) & 0xff, rgb & 0xff));
    if (hsv.s > 10) hue = hsv.h;  // white has no hue: keep the last one
  } else if (!strcmp(sub, "v")) {
    const char *ps = strstr(message, "<ps>");
    if (ps) {
      int p = atoi(ps + 4);
      preset = p >= 1 && p <= presets_max ? p : 0;
    }
  } else {
    return;
  }
  need_refresh = true;
}

// return false if we removed ourselves from the connected devices list
bool tab_mqtt_leds::hardware_changed(void) {
  need_refresh = true;
  if (last_change == D_CONNECTING) {
    printf_log("Connecting\n");
  } else if (last_change == D_CONNECTED) {
    tab_create();
    send_sync_data(SYNC_START);
    send_sync_data(SYNC_ON);
    printf_log("Connected\n");
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", gettabname());
    send_sync_data(SYNC_BYE);
    return false;
  }
  return true;
}