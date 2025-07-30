#pragma once

#include "lvgl-utils.h"
#include "tab.hpp"

class tab_lovense : public Tab {
 public:
  tab_lovense();
  ~tab_lovense();
  void encoder_change(int sw, int change) override;
  void switch_change(int sw, boolean state) override;
  void loop(boolean activetab) override;
  void focus_change(boolean focus) override;
  boolean hardware_changed(void) override;
  void gotsyncdata(Tab *t, sync_data status) override;

  enum main_modes {
    MODE_MANUAL = 0,
    MODE_TIMER,
    MODE_RANDOM,
    MODE_SYNC
  };
  const char *lovense_main_modes_c = "Manual\nTimer\nRandom\nSync";
  const char *lovense_presets = "Continuous\nMode0\nRampDown\nFastPulses\nMode3\nMode4\nMode5\nMode6\nMode7\nMode8\nMode9\nMode10";
  const int lovense_max_presets = 12;

  int getcyclecount(void) override { return thrustcount; };

  main_modes main_mode;
  int main_pattern = 0;
  bool need_refresh = false;

 private:
  lv_obj_t *tab_status;
  int thrustcount = 0;
  void tab_create(void);
  void tab_create_status(lv_obj_t *tv2);
  bool need_knob_refresh = false;
  bool ison;
  bool lockpanel = false;
  int knob_speed, knob_tempo;
  int timermillis = 0;
  unsigned long tempotimer = 0;

};
