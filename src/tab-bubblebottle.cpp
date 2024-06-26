#include "tab.hpp"
#include "lvgl-utils.h" // for printf_log()
#include "device-bubblebottle.hpp" 
#include "tab-bubblebottle.hpp"

tab_bubblebottle::tab_bubblebottle() {
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
}

tab_bubblebottle::~tab_bubblebottle() {
}

void tab_bubblebottle::loop(boolean activetab) {
  device_bubblebottle *md = static_cast<device_bubblebottle *>(device);
  int state;
  if (xQueueReceive(md->events,&state, 0)) {
    printf_log("Bubbler breath %s\n",state==1?"in":"release");
    send_sync_data(state==1?SYNC_ON:SYNC_OFF);
  }
}
  
// return false if we removed ourselves from the connected devices list
boolean tab_bubblebottle::hardware_changed(void) {
  if (last_change == D_CONNECTING) {
    printf_log("Connecting %s\n", device->getShortName());
  } else if (last_change == D_CONNECTED) {
    device_bubblebottle *md = static_cast<device_bubblebottle *>(device);
    send_sync_data(SYNC_START);
    send_sync_data(SYNC_OFF);
    xQueueReset(md->events);
    printf_log("Connected %s\n", device->getShortName());
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", device->getShortName());
    send_sync_data(SYNC_BYE);
    return false;
  }
  return true;
}
