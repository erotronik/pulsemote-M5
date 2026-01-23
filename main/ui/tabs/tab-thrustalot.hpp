#pragma once

#include "lvgl-utils.h"
#include "tab.hpp"
#include "tab-object-status.hpp"

class tab_thrustalot : public Tab {
 public:
  tab_thrustalot();
  ~tab_thrustalot();
  void encoder_change(int sw, int change) override;
  void switch_change(int sw, bool state) override;
  void loop(bool activetab) override;
  void focus_change(bool focus) override;
  bool hardware_changed(void) override;
  void gotsyncdata(Tab *t, sync_data status) override;

  enum main_modes {
    MODE_MANUAL = 0,
    MODE_TIMER,
    MODE_RANDOM,
    MODE_SYNC
  };
  const char *thrustalot_main_modes_c = "Manual\nTimer\nRandom\nSync";

  int getcyclecount(void) override { return thrustcount; };

  main_modes main_mode;
  bool need_refresh = false;

 private:
  tab_object_status *status;
  int thrustcount = 0;
  void tab_create(void);
  bool need_knob_refresh = false;
  bool ison;
  bool lockpanel = false;
  int knob_speed, knob_tempo;
  int timermillis = 0;
  unsigned long tempotimer = 0;
  int tempo_to_ms(int x);

};
