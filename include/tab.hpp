#pragma once

#include <LinkedList.h>
#include <lvgl.h>

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

  // Called when a physical push switch is pushed or released
  virtual void switch_change(int sw, boolean state) {};

  // Called when a physical encoder knob is changed
  virtual void encoder_change(int sw, int change) {};

  // Tab can have a loop function that is called periodically, but don't rely on the timing
  virtual void loop(boolean activetab) {};

  // Called when a tab gets the focus so it can update it's display if needed
  virtual void focus_change(boolean focus) {};

  // Called once when a tab is set up for the first time
  virtual void setup(void) {};

  // Called when we get data on the sync bus
  virtual void gotsyncdata(Tab *t, sync_data syncdata) {};

  // Callback for when hardware state has changed
  virtual boolean hardware_changed(void) { return true; };

  // A tab returns the number of on/off cycles that have happened, but tabs can override it to count other events
  virtual int getcyclecount(void) { return cyclecount; };

  // A tab can return a LV_SYMBOL (or more than one) displayed on the splashscreen, for example a wifi symbol
  virtual const char *geticons(void) { return ""; };

  // The short name we display for the tab, by default it comes from the device but a tab can override it
  virtual const char *gettabname(void) { 
    if (device) return device->getShortName();
    return "";
  };

  // Send sync data from our tab to all the others
  virtual void send_sync_data(sync_data syncstatus) {
    if (syncstatus == SYNC_OFF) cyclecount++;
    for (int i=0; i<tabs.size(); i++) {
      Tab *st = tabs.get(i);
      if (st != this) {
        st->gotsyncdata(this,syncstatus);
      }
    }
  };

  DeviceType type;
  lv_obj_t *page;  // the content of the tab, don't use tab_id as we need to remove tabs
  type_of_change old_last_change;
  type_of_change last_change;
  Device *device;
  tab_object_buttonbar *buttonbar;

  private:
    // See getcyclecount()
    int cyclecount = 0;

};