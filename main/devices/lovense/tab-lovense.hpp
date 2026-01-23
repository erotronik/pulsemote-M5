#pragma once

#include "lvgl-utils.h"
#include "tab.hpp"

class tab_lovense : public Tab {
 public:
  tab_lovense();
  ~tab_lovense();
  void encoder_change(int sw, int change) override;
  void switch_change(int sw, bool state) override;
  void loop(bool activetab) override;
  void focus_change(bool focus) override;
  bool hardware_changed(void) override;
  void gotsyncdata(Tab *t, sync_data status) override;

  const char *lovense_main_modes_c = "Manual\nTimer\nRandom\nSync";

  int getcyclecount(void) override { return thrustcount; };

  int main_pattern = 0;

 private:
  lv_obj_t *tab_battery;
  int thrustcount = 0;
  void tab_create(void);
  void tab_create_battery(lv_obj_t *tv2);
  bool need_knob_refresh = false;
  bool ison;
  bool lockpanel = false;
  int knob_speed, knob_tempo;
  unsigned long tempotimer = 0;
  time_t battery_time = 0;
  int battery_pc = 0;

};
