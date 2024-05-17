#pragma once

#include "tab-object-timer.hpp"
#include "tab-object-sync.hpp"
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include "tab.hpp"

class tab_thrustalot : public Tab {
 public:
  tab_thrustalot();
  ~tab_thrustalot();
  void encoder_change(int sw, int change) override;
  void switch_change(int sw, boolean state) override;
  void loop(boolean activetab) override;
  void focus_change(boolean focus) override;
  boolean hardware_changed(void) override;
  void gotsyncdata(Tab *t, sync_data status) override;
  tab_object_timer *rand_timer;
  tab_object_timer *timer;
  tab_object_sync *sync;

  enum main_modes {
    MODE_MANUAL = 0,
    MODE_TIMER,
    MODE_RANDOM,
    MODE_SYNC
  };

  main_modes main_mode;
  bool need_refresh = false;

 private:
  lv_obj_t *tab_status;
  void tab_create(void);
  void tab_create_status(lv_obj_t *tv2);
  bool need_knob_refresh = false;
  bool ison;
  bool lockpanel = false;
  int knob_speed, knob_tempo;
  int timermillis = 0;
  unsigned long tempotimer = 0;
  int tempo_to_ms(int x);

};
