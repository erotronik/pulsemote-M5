#pragma once

#include "lvgl-utils.h"
#include "tab.hpp"
#include "tab-object-segbar.hpp"
#include "device-ossm.hpp"

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
  std::string get_status_text(void) override;

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

  // Getting the OSSM into Stroke Engine, and keeping it in step with us once it's there
  std::string ossm_state;
  bool reached_engine = false;  // it's been in Stroke Engine since we connected
  unsigned long last_go = 0;
  void follow_state(device_ossm *md);
  void send_settings(device_ossm *md);
  const char *state_notice(void);
};
