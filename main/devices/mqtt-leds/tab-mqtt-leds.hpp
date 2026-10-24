#pragma once

#include "lvgl-utils.h"
#include "tab.hpp"

class tab_mqtt_leds : public Tab {
 public:
  tab_mqtt_leds(char *n, char *t);
  ~tab_mqtt_leds();
  void encoder_change(int sw, int change) override;
  void switch_change(int sw, bool state) override;
  void loop(bool activetab) override;
  void focus_change(bool focus) override;
  bool hardware_changed(void) override;
  void gotsyncdata(Tab *t, sync_data status) override;
  void mqtt_received(const char *topic, const char *message) override;

  void setup() override {
    tab_create();
  }

  const char* gettabname(void) override { return mqtt_topic_name;};

  const char *mqtt_leds_main_modes_c =  "Manual\nTimer\nRandom\nSync";

 private:
  char mqtt_topic[100] = "";
  char mqtt_topic_name[20] = "";
  void tab_create(void);
  bool need_knob_refresh = false;
  int hue = 0;  // 0-359
  time_t huesend;
  int brightness = 128;  // 1-255, WLED's last brightness while on
  unsigned long brightnesssend = 0;
  unsigned long last_turned = 0;  // when a knob was last turned, so WLED's replies don't fight it
  int preset = 1;  // 1-presets_max, or 0 for none
  static const int presets_max = 10;
  void send_preset(void);
  bool ison;
  bool lockpanel = false;
};
