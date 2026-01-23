#pragma once

#include "lvgl-utils.h"
#include "tab.hpp"
#include "tab-object-status.hpp"
#include "tab-object-modetimer.hpp"

class tab_mqtt_socket : public Tab {
 public:
  tab_mqtt_socket(char *n, char *t);
  ~tab_mqtt_socket();
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
  const char *mqtt_socket_main_modes_c =  "Manual\nTimer\nRandom\nSync";


  main_modes main_mode;
  bool need_refresh = false;
  tab_object_status *status;
  tab_object_modetimer *modetimer;

 private:
  char mqtt_topic[100] = "";
  char mqtt_topic_name[20] = "";
  void tab_create(void);
  bool need_knob_refresh = false;
  uint8_t preset = 0;
  bool ison;
  bool lockpanel = false;
  int level_a, level_b;
};
