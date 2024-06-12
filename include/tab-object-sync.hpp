#pragma once

#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>

class tab_object_sync {
 public:
  tab_object_sync() {};

  // This may get more complex later as you might want to dynamically see what is providing
  // sync data and allow the user to switch between them
  
  void view(lv_obj_t *parent);

  void show(bool show);

  bool isinverted();

 private:
  int value[5];
  static void event_handler(lv_event_t* e);
  bool moderandom;
  lv_obj_t* sw;
  lv_obj_t* container;
};