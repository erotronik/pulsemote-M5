#pragma once

#include <LinkedList.h>
#include <lvgl.h>

#include "device-coyote2.hpp"
#include "device-mk312.hpp"
#include "device.hpp"

#define COLOUR_RED 0x882211
#define COLOUR_GREEN 0x118822

extern lv_obj_t *tv;

class Tab {
 public:
  enum sync_data {
    SYNC_START =0, SYNC_ON, SYNC_OFF
  };

  virtual void switch_change(int sw, boolean state) {};
  virtual void encoder_change(int sw, int change) {};
  virtual void loop(boolean activetab) {};
  virtual void focus_change(boolean focus) {};
  virtual void setup(void) {};
  virtual void gotsyncdata(Tab *t, sync_data syncdata) {};
  virtual boolean hardware_changed(void) { return true; };

  DeviceType type;
  lv_obj_t *page;  // the content of the tab, don't use tab_id as we need to
                   // remove tabs
  type_of_change old_last_change;
  type_of_change last_change;
  Device *device;
};

extern LinkedList<Tab *> tabs;

void printf_log(const char *format, ...);

// given a tabview and a content page of a tab, get the tab index. This
// stops us having to keep track of the tabid which changes when we
// delete tabs as hardware goes away

inline int lv_get_tabview_idx_from_page(lv_obj_t *l, lv_obj_t *page) {
  lv_obj_t *c = lv_tabview_get_content(l);
  for (int i = 0; i < lv_obj_get_child_count(c); i++) {
    if (lv_obj_get_child(c, i) == page) return i;
  }
  return -1;
}
