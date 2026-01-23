#pragma once

#include "lvgl-utils.h"
#include "tab.hpp"
#include "tab-object-status.hpp"

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

  enum main_modes {
    MODE_MANUAL = 0,
    MODE_TIMER,
    MODE_RANDOM,
    MODE_SYNC
  };
  const char *ossm_main_modes_c = "Manual\nTimer\nRandom\nSync";

  main_modes main_mode;
  int main_pattern = 0;
  bool need_refresh = false;

 private:
  tab_object_status *status;
  int thrustcount = 0;
  bool firstdata = false;
  void tab_create(void);
  bool need_knob_refresh = false;
  bool ison;
  bool lockpanel = false;
  int knob_speed, knob_stroke, knob_depth, knob_sensation;
  int timermillis = 0;
  unsigned long tempotimer = 0;

  typedef struct {
    lv_obj_t *base;   // green background
    lv_obj_t *seg;    // red segment
    int width;        // total width in px (100 here)
    int height;       // total height in px (5 here)
  } segbar_t;

  segbar_t mybar;
  void segbar_create(lv_obj_t *parent, segbar_t *bar, int x, int y);
  void segbar_set(segbar_t *bar, int x, int y);
};
