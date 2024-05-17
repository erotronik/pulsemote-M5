#pragma once

#include "tab.hpp"
#include "lvgl-utils.h"
#include "device-bubblebottle.hpp"

class tab_bubblebottle : public Tab {
 public:
  tab_bubblebottle() {
    page = nullptr;
    old_last_change = last_change = D_NONE;
    device = nullptr;
  };
  ~tab_bubblebottle();

  void loop(boolean activetab) override {
    device_bubblebottle *md = static_cast<device_bubblebottle *>(device);
    if (md && md->bottle_changed_state) {
      printf_log("Bubbler breath %s\n",md->bottle_state==1?"in":"release");
      md->bottle_changed_state = false;
    }
  };
  boolean hardware_changed(void) override{
// return false if we removed ourselves from the connected devices list
    if (last_change == D_CONNECTING) {
      printf_log("Connecting %s\n", device->getShortName());
    } else if (last_change == D_CONNECTED) {
      //send_sync_data(SYNC_START);
      //send_sync_data(SYNC_OFF);
      //ison = false;
      printf_log("Connected %s\n", device->getShortName());
    } else if (last_change == D_DISCONNECTED) {
      printf_log("Disconnected %s\n", device->getShortName());
      return false;
    }
    return true;
  };

 private:

};
