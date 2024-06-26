#include <M5Unified.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <memory>

#include "comms-bt.hpp"

#include <esp_timer.h>

#include "m5io.h"
#include "tab-coyote.hpp"
#include "tab-mk312.hpp"
#include "tab-thrustalot.hpp"
#include "tab-splashscreen.hpp"
#include "tab-bubblebottle.hpp"
#include "tab-dgbutton.hpp"
#include "tab-loop.hpp"
#include "tab-mqtt.hpp"
#include "tab.hpp"
#include "lvgl-utils.h"

lv_obj_t *tv;
lv_display_t *display;
lv_indev_t *indev;

std::list<Tab*> tabs;

byte lastencodervalue[numencoders] = {128, 128, 128, 128};
byte encodervalue[numencoders] = {128, 128, 128, 128};

void RotaryEncoderChanged(bool clockwise, int id) {
  if (clockwise) {
    encodervalue[id]++;
  } else {
    encodervalue[id]--;
  }
}

// called from main, look to see if we've got any button pushes
// from the interrupt queue and dispatch them to the callback of
// the device with current open tab

void handlebuttonpushes() {
  event_t received_event;
  byte count = 4;  // a few callbacks allowed per loop
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

// called from main, look to see if we've got any rotary encoder
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
  ESP_LOGD("main", "tabview cb %s on %d: current tab %d",
           pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(),
           lv_tabview_get_tab_act(tv));
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
  lv_obj_set_style_text_font(lv_tabview_get_tab_bar(tv), &lv_font_montserrat_16, LV_PART_MAIN);
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
  ESP_LOGD("main", "change handler task called from %s on %d",
           pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

  bool newdevice = true;
  for (const auto& tt : tabs) {
    if (d && tt->device == d) { // found an existing tab that matches the device instance
      ESP_LOGD("main","matched an existing tab %s",tt->gettabname());
      tt->last_change = t;
      newdevice = false;
      break;
    }
  }
  if (newdevice && t != D_DISCONNECTED) {
    ESP_LOGD("main", "a new device has appeared");
    DeviceType type = d->getType();
    Tab *ta = nullptr;
    if (type == DeviceType::device_mk312) {  // bootstrap
      ta = new tab_mk312();
    } else if (type == DeviceType::device_coyote2) {
      ta = new tab_coyote();
    } else if (type == DeviceType::device_thrustalot) {
      ta = new tab_thrustalot();
    } else if (type == DeviceType::device_bubblebottle) {
      ta = new tab_bubblebottle();
    } else if (type == DeviceType::device_dgbutton) {
      ta = new tab_dgbutton();      
    } else if (type == DeviceType::device_loop) {
      ta = new tab_loop();    
    }
    if (ta != nullptr) {
      ta->type = type;
      ta->device = d;
      ta->last_change = t;
      ta->needssetup = true;
      tabs.emplace_back(ta);
    }
  }
}

void TaskMain(void *pvParameters);

void setup() {
  M5.begin();
  printf_log("begin done\n");

  lv_init();
  printf_log("lv_init done\n");

  lv_tick_set_cb(lvgl_tick_function);

  display = lv_display_create(SCREENW, SCREENH);
  lv_display_set_flush_cb(display, lvgl_display_flush);
  static lv_color_t buf1[SCREENW * 15];
  lv_display_set_buffers(display, buf1, nullptr, sizeof(buf1),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, lvgl_touchpad_read);
  printf_log("display setup done\n");

  setup_tabs();
  printf_log("tab setup done\n");

  m5io_init();
  printf_log("io setup done\n");

  printf_log("Version %s\n",__DATE__);
  printf_log("Scanning for devices...\n");

  xTaskCreatePinnedToCore(TaskMain, "Main", 1024 * 20, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(TaskCommsBT, "comms-bt", 1024 * 20, nullptr, 2, nullptr, 1);
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
      ESP_LOGD("main", "%s changed state: %d %d", t->device->getShortName(), (int)t->last_change, (int)t->old_last_change);
      if (!t->hardware_changed()) {
        // false means the device has gone away, get rid of the tab
        ESP_LOGD("main","removing tab");
        lv_hide_tab(t->page);
        st = tabs.erase(st);
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

void main_loop() {
  M5.update();
  lv_task_handler();
  handlehardwarecallbacks();
  handlebuttonpushes();
  handlerotaryencoders();
  handletabloops();
  vTaskDelay(1);
}

void loop() {}; // We use FreeRTOS tasks instead

void TaskMain(void *pvParameters) {
  vTaskDelay(200);
  ESP_LOGD("main", "Main task started: %s on %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());
  while (true) main_loop();
}
