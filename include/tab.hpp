#pragma once

#include <LinkedList.h>
#include <lvgl.h>

#include "device-coyote2.hpp"
#include "device.hpp"
#include "tab-object-buttonbar.hpp"

#define COLOUR_RED 0x882211
#define COLOUR_GREEN 0x118822

extern lv_obj_t *tv;

class Tab;
extern LinkedList<Tab *> tabs;

class Tab {
 public:
  enum sync_data {
    SYNC_START =0, SYNC_ON, SYNC_OFF, SYNC_BYE
  };

  virtual void switch_change(int sw, boolean state) {};
  virtual void encoder_change(int sw, int change) {};
  virtual void loop(boolean activetab) {};
  virtual void focus_change(boolean focus) {};
  virtual void setup(void) {};
  virtual void gotsyncdata(Tab *t, sync_data syncdata) {};
  virtual boolean hardware_changed(void) { return true; };

  tab_object_buttonbar *buttonbar;

  DeviceType type;
  lv_obj_t *page;  // the content of the tab, don't use tab_id as we need to
                   // remove tabs
  type_of_change old_last_change;
  type_of_change last_change;
  Device *device;
  int cyclecount = 0;

  virtual void send_sync_data(sync_data syncstatus) {
    if (syncstatus == SYNC_OFF) cyclecount++;
    for (int i=0; i<tabs.size(); i++) {
      Tab *st = tabs.get(i);
      if (st != this) {
        st->gotsyncdata(this,syncstatus);
      }
    }
  };

};

