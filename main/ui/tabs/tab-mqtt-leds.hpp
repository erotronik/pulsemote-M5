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

  void setup() override {
    tab_create();
  }

  const char* gettabname(void) override { return mqtt_topic_name;};

  enum main_modes {
    MODE_MANUAL = 0,
    MODE_TIMER,
    MODE_RANDOM,
    MODE_SYNC
  };
  const char *mqtt_leds_main_modes_c =  "Manual\nTimer\nRandom\nSync";

  main_modes main_mode;
  bool need_refresh = false;

 private:
  char mqtt_topic[100] = "";
  char mqtt_topic_name[20] = "";
  lv_obj_t *tab_status;
  void tab_create(void);
  void tab_create_status(lv_obj_t *tv2);
  bool need_knob_refresh = false;
  uint8_t hue = 0;
  time_t huesend;
  int preset = 0;
  bool ison;
  bool lockpanel = false;
  int timermillis = 0;
};
