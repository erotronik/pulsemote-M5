#pragma once

#include "lvgl-utils.h"

class tab_object_timer;

class tab_object_timer {
 public:
  tab_object_timer(bool moderandom, bool on_only = false);
  bool highlight_next_field(void);
  bool has_focus(void);
  void show(bool x);
  int gettimeon(void);
  int gettimeoff(void);
  void rotary_change(int change);
  void set_max_on(int max_seconds) { max_on = max_seconds; }
  void view(lv_obj_t* tv2);

 private:
  int value[5];
  int max_on = 0;
  static void event_handler(lv_event_t* e);
  bool moderandom;
  bool mode_on_only;
  lv_obj_t* active_btn;
  lv_obj_t* container;
  bool is_visible;
};