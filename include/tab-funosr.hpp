#pragma once

#include "lvgl-utils.h"
#include "tab.hpp"

class tab_funosr : public Tab {
 public:
  tab_funosr();
  ~tab_funosr();
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
  const char *funosr_main_modes_c = "Manual\nTimer\nRandom\nSync";
  main_modes main_mode;
  bool need_refresh = false;

 private:
  lv_obj_t *tab_status;
  int main_pattern = 0;
  void tab_create(void);
  void tab_create_status(lv_obj_t *tv2);
  bool need_knob_refresh = false;
  bool ison;
  int knob_speed, knob_stroke, knob_depth;
  int timermillis = 0;
  void send_funosr();
  void send_stop();
  void goto_min();
  void goto_max();

  char msg[200];

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
