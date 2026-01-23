#pragma once

#include <lvgl.h>
#include <list>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "device.hpp"
#include "tab-object-buttonbar.hpp"
#include "tab-object-timer.hpp"
#include "tab-object-sync.hpp"
#include "tab-object-modes.hpp"
#include "tab-object-patterns.hpp"
#include "tab-object-status.hpp"
#include "tab-object-modetimer.hpp"

#define COLOUR_RED 0x882211
#define COLOUR_GREEN 0x118822

extern lv_obj_t *tv;

class Tab;

extern std::list<Tab *> tabs;
extern SemaphoreHandle_t tabs_mutex;
extern SemaphoreHandle_t lvgl_mutex;

struct TabLock {
    SemaphoreHandle_t _sem;
    bool _locked = false;
    TabLock(SemaphoreHandle_t sem, TickType_t timeout = portMAX_DELAY) : _sem(sem) {
        if (_sem) {
            // If the scheduler is suspended, we MUST NOT block.
            if (xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED) {
                timeout = 0;
            }
            if (xSemaphoreTakeRecursive(_sem, timeout) == pdTRUE) {
                _locked = true;
            }
        }
    }
    ~TabLock() {
        if (_locked && _sem) xSemaphoreGiveRecursive(_sem);
    }
    operator bool() const { return _locked; }
};
class Tab {
 public:
  enum sync_data {
    SYNC_START =0, SYNC_ON, SYNC_OFF, SYNC_BYE, SYNC_ALLOFF, SYNC_BUTTONPRESS, SYNC_BUTTONRELEASE
  };
    
  enum main_modes {
    MODE_MANUAL = 0,
    MODE_TIMER,
    MODE_RANDOM,
    MODE_SYNC
  };

  // Called when a physical push switch is pushed or released
  virtual void switch_change(int sw, bool state) {};

  // Called when a physical encoder knob is changed
  virtual void encoder_change(int sw, int change) {};

  // Tab can have a loop function that is called periodically, but don't rely on the timing
  virtual void loop(bool activetab) {};

  // Called when a tab gets the focus so it can update it's display if needed
  virtual void focus_change(bool focus) {};

  // Called once when a tab is set up for the first time
  virtual void setup(void) {};

  // Called when we get data on the sync bus
  virtual void gotsyncdata(Tab *t, sync_data syncdata) {};

  // Callback for when hardware state has changed
  virtual bool hardware_changed(void) { return true; };

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
    if (syncstatus == SYNC_OFF || syncstatus == SYNC_ALLOFF) cyclecount++;
    TabLock lock(tabs_mutex);
    if (lock) {
      for (const auto& item : tabs) {
        if (item != this) {
          item->gotsyncdata(this,syncstatus);
        }
      }
    }
  };

  DeviceType type;
  lv_obj_t *page = NULL;  // the content of the tab, don't use tab_id as we need to remove tabs
  type_of_change old_last_change;
  type_of_change last_change;
  Device *device;
  tab_object_buttonbar *buttonbar = nullptr;
  bool needssetup = false;
  bool need_refresh = false;
  bool need_knob_refresh = false;

  main_modes main_mode = MODE_MANUAL;
  tab_object_status *status = nullptr;
  tab_object_modetimer *modetimer = nullptr;

  tab_object_modes *modeselect;
  tab_object_sync *sync;
  tab_object_timer *rand_timer;
  tab_object_timer *timer;
  tab_object_patterns *patternselect;

  private:
    // See getcyclecount()
    int cyclecount = 0;

};
