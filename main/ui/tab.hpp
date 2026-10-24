#pragma once

#include <lvgl.h>
#include <list>
#include <string>
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
  virtual ~Tab() {}

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

  // The device's output levels, 0-100, for Home Assistant (mqtt-ha.cpp). Returns how many. By default
  // it's the main knobs; a tab with levels that aren't all on them overrides it
  struct level_t {
    char name[16];  // like "1", "A" or "Speed", from the knob's label
    int value;
  };
  static const int levels_max = 8;
  virtual int get_levels(level_t levels[levels_max]);

  // The device's battery level 0-100, or -1 if unknown, for Home Assistant. By default what
  // set_battery_label() was given
  virtual int get_battery(void) { return shown_battery; }

  // What the device is doing, for Home Assistant: by default the first line of the status box (not
  // the timer countdown under it), empty if there's no status box
  virtual std::string get_status_text(void);

  // True for a button device, which sends button events to Home Assistant (mqtt-ha.cpp)
  virtual bool has_button_events(void) { return false; }

  // A message from the broker, which every tab sees (from the main loop). For a tab that subscribed
  // to something with mqttsubscribe()
  virtual void mqtt_received(const char *topic, const char *message) {}

  // A device with an on/off reading of its own, like the bubble bottle's breathing, which shows in
  // Home Assistant as a binary sensor with this name. nullptr for none
  virtual const char *activity_name(void) { return nullptr; }
  virtual bool get_activity(void) { return false; }

  // And a rate per minute, like breaths per minute, as a sensor with this name. nullptr for none;
  // get_rate() is -1 while it isn't known yet
  virtual const char *rate_name(void) { return nullptr; }
  virtual float get_rate(void) { return -1; }

  lv_obj_t* create_standard_page(const char* modes_str);
  void create_standard_widgets();
  // A box under the status showing the device's battery level, for devices that report one. It stays
  // hidden until set_battery_label() is given a level
  void create_battery_label(lv_obj_t *parent);
  void set_battery_label(int percent);  // -1 if unknown
  void update_standard_mode_visibility();
  static void standard_mode_change_cb(lv_event_t* event);

  DeviceType type;
  lv_obj_t *page = NULL;  // the content of the tab, don't use tab_id as we need to remove tabs
  type_of_change old_last_change;
  type_of_change last_change;
  Device *device;
  tab_object_buttonbar *buttonbar = nullptr;
  bool needssetup = false;
  bool need_refresh = false;
  bool need_knob_refresh = false;
  lv_obj_t *battery_label = nullptr;
  int shown_battery = -1;

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
