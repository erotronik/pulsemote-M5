#pragma once

#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>

class tab_object_timer;

class tab_object_timer {
 public:
  tab_object_timer(bool moderandom);
  void highlight_next_field(void);
  bool has_active_button(void);
  void show(bool x);
  int gettimeon(void);
  int gettimeoff(void);
  void rotary_change(int change);
  lv_obj_t* view(lv_obj_t* tv2);

 private:
  int value[5];
  static void event_handler(lv_event_t* e);
  bool moderandom;
  lv_obj_t* active_btn;
  lv_obj_t* container;
};