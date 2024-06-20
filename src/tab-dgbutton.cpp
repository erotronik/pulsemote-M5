#include "tab.hpp"
#include "lvgl-utils.h" // for printf_log()
#include "device-dgbutton.hpp" 
#include "tab-dgbutton.hpp"
#include <M5Unified.h>

tab_dgbutton::tab_dgbutton() {
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
}

tab_dgbutton::~tab_dgbutton() {
}

void tab_dgbutton::loop(boolean activetab) {
  device_dgbutton *md = static_cast<device_dgbutton *>(device);
  device_dgbutton::dgbutton_event state;
  if (xQueueReceive(md->events,&state, 0)) {
    if (state == device_dgbutton::dgbutton_event::HELD) {
      printf_log("DG Button Pushed\n");
      send_sync_data(SYNC_ALLOFF);
      M5.Speaker.begin();
      M5.Speaker.tone(660, 100);
      M5.Speaker.tone(2000, 1000);
    }
  }
}
  
// return false if we removed ourselves from the connected devices list
boolean tab_dgbutton::hardware_changed(void) {
  if (last_change == D_CONNECTING) {
    printf_log("Connecting %s\n", device->getShortName());
  } else if (last_change == D_CONNECTED) {
    device_dgbutton *md = static_cast<device_dgbutton *>(device);
    xQueueReset(md->events);
    printf_log("Connected %s\n", device->getShortName());
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", device->getShortName());
    return false;
  }
  return true;
}
