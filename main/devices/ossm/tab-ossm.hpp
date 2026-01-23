#pragma once

#include "lvgl-utils.h"
#include "tab.hpp"
#include "tab-object-segbar.hpp"

class tab_ossm : public Tab {
 public:
  tab_ossm();
  ~tab_ossm();
  void encoder_change(int sw, int change) override;
  void switch_change(int sw, bool state) override;
  void loop(bool activetab) override;
  void focus_change(bool focus) override;
  bool hardware_changed(void) override;
  void gotsyncdata(Tab *t, sync_data status) override;

  const char *ossm_main_modes_c = "Manual\nTimer\nRandom\nSync";

  int main_pattern = 0;
  tab_object_segbar *segbar;

 private:
  int thrustcount = 0;
  bool firstdata = false;
  void tab_create(void);
  bool need_knob_refresh = false;
  bool ison;
  bool lockpanel = false;
  int knob_speed, knob_stroke, knob_depth, knob_sensation;
};
