#pragma once

#include "lvgl-utils.h"
#include "tab.hpp"

class tab_mk312 : public Tab {
 public:
  tab_mk312();
  ~tab_mk312();
  void encoder_change(int sw, int change) override;
  void switch_change(int sw, bool state) override;
  void loop(bool activetab) override;
  void focus_change(bool focus) override;
  bool hardware_changed(void) override;
  void gotsyncdata(Tab *t, sync_data status) override;
  int wanted_mode = 0;

  const char *mk312_main_modes_c = "Manual\nTimer\nRandom\nSync";



  
 private:
  void tab_create(void);
  bool need_knob_refresh = false;
  bool ison;
  bool lockpanel = false;
  int level_a, level_b;
  int last_level_button_press_a = 0;
  int last_level_button_press_b = 0;
};
