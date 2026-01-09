#include "comms-bt.hpp"

#include "hardware-tft.h"
#include "hardware-io-m5.h"
#include "tab-coyote.hpp"
#include "tab-mk312.hpp"
#include "tab-thrustalot.hpp"
#include "tab-funosr.hpp"
#include "tab-lovense.hpp"
#include "tab-splashscreen.hpp"
#include "tab-ossm.hpp"
#include "tab-bubblebottle.hpp"
#include "tab-dgbutton.hpp"
#include "tab-loop.hpp"
#include "tab-mqtt.hpp"
#include "tab.hpp"
#include "lvgl-utils.h"

lv_obj_t *tv;

std::list<Tab*> tabs;

byte lastencodervalue[numencoders] = {128, 128, 128, 128};
byte encodervalue[numencoders] = {128, 128, 128, 128};

void RotaryEncoderChanged(bool clockwise, int id) {
  encodervalue[id] += clockwise ? 1 : -1;
}

// called from main loop, look to see if we've got any button pushes
// from the interrupt queue and dispatch them to the callback of
// the device with current open tab

void handlebuttonpushes() {
  event_t received_event;
  byte count = 4;  // a few callbacks allowed per loop, arbitary
  while (count > 0 && xQueueReceive(event_queue, &received_event, 0)) {
    // find what device tab is active as physical buttons must only work on active tab
    lv_obj_t *activepage = lv_obj_get_child(lv_tabview_get_content(tv),lv_tabview_get_tab_act(tv));
    for (const auto& t : tabs) {
      if (activepage == t->page) 
        t->switch_change(received_event.target, received_event.value);
    }
    count--;
  }
}

// called from main loop, look to see if we've got any rotary encoder
// changes and dispatch them to the callback of the device with current open tab

void handlerotaryencoders() {
  for (int i = 0; i < numencoders; i++) {
    int change = encodervalue[i] - lastencodervalue[i];
    if (change != 0) {
      lastencodervalue[i] = encodervalue[i] = 128;
      // find what device tab is active as encoders must only work on active tab
      lv_obj_t *activepage = lv_obj_get_child(lv_tabview_get_content(tv),lv_tabview_get_tab_act(tv));
      for (const auto& t : tabs) {
        if (activepage == t->page)
          t->encoder_change(i, change);
      }
    }
  }
}

// this is called when the tab changes, by click or swipe.  trigger the
// focus callback of the newly active tab (don't bother to tell the
// old tab we've gone away, could be added later if needed)

void tabview_event_cb(lv_event_t *event) {
    ESP_LOGD("main", "tabview cb %s on %d: current tab %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), lv_tabview_get_tab_act(tv));
    lv_obj_t *activepage = lv_obj_get_child(lv_tabview_get_content(tv),lv_tabview_get_tab_act(tv));
    for (const auto& t : tabs) {
      if (activepage == t->page)
        t->focus_change(true);  // true means is the new active tab
    }
}

// set up the tab view and create the first default tab, the
// welcome screen (splashscreen)

void setup_tabs(void) {
  tv = lv_tabview_create(lv_screen_active());
  lv_obj_set_scrollbar_mode(lv_tabview_get_content(tv), LV_SCROLLBAR_MODE_OFF); // uses bottom few pixels and not needed 
  lv_obj_set_style_text_font(lv_tabview_get_tab_bar(tv), &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_add_event_cb(tv, tabview_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_tabview_set_tab_bar_size(tv, 34);

  Tab *sp = new tab_splashscreen();
  sp->setup();
  tabs.emplace_back(sp);

#ifdef CONFIG_WIFI_SSID
  Tab *mq = new tab_mqtt();
  mq->setup();
  tabs.emplace_back(mq);
#endif
}

// This is called when our scanner detects a new device; figure out
// the appropriate tab we need to handle that device and add it
//
// it's a callback so don't do any actual GUI stuff here, just set up structures

void device_change_handler(type_of_change t, Device *d) {
  ESP_LOGD("main", "change handler task called from %s on %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

  for (const auto& tt : tabs) {
    if (d && tt->device == d) { // found an existing tab that matches the device instance
      ESP_LOGD("main","matched an existing tab %s",tt->gettabname());
      tt->last_change = t;
      return;
    }
  }
  if (t == D_DISCONNECTED) return;

  ESP_LOGD("main", "a new device has appeared");
  Tab *ta = nullptr;
  switch (d->getType()) {
    case DeviceType::device_mk312:        ta = new tab_mk312(); break;
    case DeviceType::device_coyote:       ta = new tab_coyote(); break;
    case DeviceType::device_funosr:       ta = new tab_funosr(); break;
    case DeviceType::device_lovense:      ta = new tab_lovense(); break;
    case DeviceType::device_ossm:         ta = new tab_ossm(); break;
    case DeviceType::device_bubblebottle: ta = new tab_bubblebottle(); break;
    case DeviceType::device_dgbutton:     ta = new tab_dgbutton(); break;
    case DeviceType::device_loop:         ta = new tab_loop(); break;
    default:                              return;
  }
  ta->type = d->getType();
  ta->device = d;
  ta->last_change = t;
  ta->needssetup = true;
  tabs.emplace_back(ta);
}

// Handle any tabs that have changed status, this includes
// cleaning up and removing a tab if it's gone away

void handlehardwarecallbacks() {
  for (auto st = tabs.begin(); st != tabs.end(); ++st) {
    Tab *t = *st;
    if (t->needssetup) {
      t->setup();
      t->needssetup = false;
    }
    if (t->old_last_change != t->last_change) {
      ESP_LOGD("main", "%s changed state: %d %d", t->gettabname(), (int)t->last_change, (int)t->old_last_change);
      if (!t->hardware_changed()) {
        // false means the device has gone away, get rid of the tab
        ESP_LOGI("main","removing tab %s", t->gettabname());
        lv_hide_tab(t->page);
        st = tabs.erase(st);
        return;
      }
      t->old_last_change = t->last_change;
    }
  }
}

// Call the loop() function for each of the tabs, note if the tab is active (currently visible)

void handletabloops(void) {
  lv_obj_t *activetab = lv_obj_get_child(lv_tabview_get_content(tv),lv_tabview_get_tab_act(tv));
  for (const auto& t : tabs) {
    t->loop((activetab == t->page));
  }
}

// Main UI loop

void main_loop() {
  hardware_tft_loop();
  lv_task_handler();
  handlehardwarecallbacks();
  handlebuttonpushes();
  handlerotaryencoders();
  handletabloops();
  vTaskDelay(1);
}

// Main loop for UI (FreeRTOS)

void TaskMain(void *pvParameters) {
  vTaskDelay(200);
  ESP_LOGD("main", "Main task started: %s on %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());
  while (true) main_loop();
}

void loop() {}; // We use FreeRTOS tasks instead

// Usual setup start

void setup() {
  hardware_tft_init();
  ESP_LOGD("setup","display setup done");
  m5io_init();
  ESP_LOGD("setup","io setup done");  
  setup_tabs();
  ESP_LOGD("setup","tab setup done");


  printf_log("Version %s\n\n",__DATE__);
  printf_log("Scanning for devices...\n");

  xTaskCreatePinnedToCore(TaskMain, "Main", 1024 * 20, nullptr, 1, nullptr, 1);
  xTaskCreatePinnedToCore(TaskCommsBT, "comms-bt", 1024 * 20, nullptr, 2, nullptr, 0); // ble networking is on core0
}
