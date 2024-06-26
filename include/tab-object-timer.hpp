#pragma once

#include "lvgl-utils.h"

class tab_object_timer;

class tab_object_timer {
 public:
  tab_object_timer(bool moderandom);
  boolean highlight_next_field(void);
  boolean has_focus(void);
  void show(bool x);
  int gettimeon(void);
  int gettimeoff(void);
  void rotary_change(int change);
  void view(lv_obj_t* tv2);

 private:
  int value[5];
  static void event_handler(lv_event_t* e);
  bool moderandom;
  lv_obj_t* active_btn;
  lv_obj_t* container;
  boolean is_visible;
};