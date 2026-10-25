#include "comms-bt.hpp"

#include "coyote/tab-coyote.hpp"
#include "mk312/tab-mk312.hpp"
#include "zc95/tab-zc95.hpp"
#include "thrustalot/tab-thrustalot.hpp"
#include "funosr/tab-funosr.hpp"
#include "lovense/tab-lovense.hpp"
#include "breath/tab-breath.hpp"
#include "tab-splashscreen.hpp"
#include "ossm/tab-ossm.hpp"
#include "bubblebottle/tab-bubblebottle.hpp"
#include "dgbutton/tab-dgbutton.hpp"
#include "loop/tab-loop.hpp"
#include "tab-mqtt.hpp"
#include "tab.hpp"
#include "lvgl-utils.h"
#include <pulsemote-pcb.hpp>
#if CONFIG_HASM5ENCODER
#include <m5encoder8.hpp>
#endif
#include "board.hpp"

lv_obj_t *tv;

std::list<Tab*> tabs;
SemaphoreHandle_t tabs_mutex = NULL;
SemaphoreHandle_t lvgl_mutex = NULL;

struct device_event_t {
  type_of_change t;
  Device *d;
};
QueueHandle_t device_event_queue = NULL;

uint8_t lastencodervalue[numencoders] = {128, 128, 128, 128};
volatile uint8_t encodervalue[numencoders] = {128, 128, 128, 128}; // counted by the encoder task

void RotaryEncoderChanged(bool clockwise, int id) {
  encodervalue[id] += clockwise ? 1 : -1;
}

// called from main loop, look to see if we've got any button pushes
// from the interrupt queue and dispatch them to the callback of
// the device with current open tab

void handlebuttonpushes() {
  event_t received_event;
  uint8_t count = 4;  // a few callbacks allowed per loop, arbitary
  TabLock lock(tabs_mutex);
  while (count > 0 && event_queue && xQueueReceive(event_queue, &received_event, 0)) {
    // find what device tab is active as physical buttons must only work on active tab
    TabLock l_lock(lvgl_mutex);
    lv_obj_t *activepage = lv_obj_get_child(lv_tabview_get_content(tv),lv_tabview_get_tab_act(tv));
    for (const auto& t : tabs) {
      if (activepage == t->page) 
        t->switch_change(received_event.target, received_event.value);
    }
    lv_display_trigger_activity(nullptr);  // for handlescreendimming()
    count--;
  }
}

// called from main loop, look to see if we've got any rotary encoder
// changes and dispatch them to the callback of the device with current open tab

void handlerotaryencoders() {
  for (int i = 0; i < numencoders; i++) {
    // Read the count once and remember it, rather than resetting it: the encoder task can add a
    // click at any moment, and resetting would lose it
    uint8_t value = encodervalue[i];
    int change = (int8_t)(uint8_t)(value - lastencodervalue[i]);
    if (change != 0) {
      lastencodervalue[i] = value;
      // find what device tab is active as encoders must only work on active tab
      TabLock lock(tabs_mutex);
      TabLock l_lock(lvgl_mutex);
      lv_obj_t *activepage = lv_obj_get_child(lv_tabview_get_content(tv),lv_tabview_get_tab_act(tv));
      for (const auto& t : tabs) {
        if (activepage == t->page)
          t->encoder_change(i, change);
      }
      lv_display_trigger_activity(nullptr);  // for handlescreendimming()
    }
  }
}

#if CONFIG_HASM5ENCODER
// An 8Encoder's knobs are the controls on screen, in order from the left. Where that's the switch,
// turning does nothing and pressing presses it. On a device's tab that's its five, the third the
// switch; on the splashscreen all eight, the last Scan

void handleencoder8() {
  // On another tab, its knobs are that tab's, which sets the LEDs of those it uses. Watched for here,
  // as lv_tabview_set_act() (the splashscreen going to a device's tab) doesn't send an event
  static lv_obj_t *lastpage = nullptr;
  {
    TabLock l_lock(lvgl_mutex);
    lv_obj_t *activepage = lv_obj_get_child(lv_tabview_get_content(tv),lv_tabview_get_tab_act(tv));
    if (activepage != lastpage)
      for (int i = 0; i <= encoder8_knobs; i++) encoder8_set_led(i, 0, 0, 0);
    lastpage = activepage;
  }

  encoder8_event_t e;
  while (encoder8_events && xQueueReceive(encoder8_events, &e, 0)) {
    TabLock lock(tabs_mutex);
    TabLock l_lock(lvgl_mutex);
    lv_obj_t *activepage = lv_obj_get_child(lv_tabview_get_content(tv),lv_tabview_get_tab_act(tv));
    for (const auto& t : tabs) {
      if (activepage != t->page || !t->buttonbar || e.input >= t->buttonbar->nbuttons) continue;
      int id = t->buttonbar->all_order[e.input];
      if (e.turn) {
        if (id != tab_object_buttonbar::switch1) t->encoder_change(id, e.turn);
      } else {
        t->switch_change(id, e.pressed);
      }
    }
    lv_display_trigger_activity(nullptr);  // for handlescreendimming()
  }
}
#endif

// this is called when the tab changes, by click or swipe.  trigger the
// focus callback of the newly active tab (don't bother to tell the
// old tab we've gone away, could be added later if needed)

void tabview_event_cb(lv_event_t *event) {
    ESP_LOGD("main", "tabview cb %s on %d: current tab %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(), lv_tabview_get_tab_act(tv));
    TabLock lock(tabs_mutex);
    TabLock l_lock(lvgl_mutex);
    lv_obj_t *activepage = lv_obj_get_child(lv_tabview_get_content(tv),lv_tabview_get_tab_act(tv));
    for (const auto& t : tabs) {
      if (activepage == t->page)
        t->focus_change(true);  // true means is the new active tab
    }
}

// set up the tab view and create the first default tab, the
// welcome screen (splashscreen)

void setup_tabs(void) {
  {
    TabLock l_lock(lvgl_mutex);
    tv = lv_tabview_create(lv_screen_active());
  }
  lv_obj_set_scrollbar_mode(lv_tabview_get_content(tv), LV_SCROLLBAR_MODE_OFF); // uses bottom few pixels and not needed 
  lv_obj_set_style_text_font(lv_tabview_get_tab_bar(tv), LV_FONT_GET(14), LV_PART_MAIN);
  lv_obj_add_event_cb(tv, tabview_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_tabview_set_tab_bar_size(tv, LV_SCALE(34));

  Tab *sp = new tab_splashscreen();
  sp->setup();
  {
    TabLock lock(tabs_mutex);
    tabs.emplace_back(sp);
  }

#ifdef CONFIG_WIFI_SSID
  Tab *mq = new tab_mqtt();
  mq->setup();
  {
    TabLock lock(tabs_mutex);
    tabs.emplace_back(mq);
  }
#endif
}

// This is called when our scanner detects a new device; figure out
// the appropriate tab we need to handle that device and add it
//
// it's a callback so don't do any actual GUI stuff here, just set up structures

void device_change_handler(type_of_change t, Device *d) {
  device_event_t event = {t, d};
  if (device_event_queue) {
    xQueueSend(device_event_queue, &event, 0);
  }
}

void process_device_events() {
  device_event_t event;
  while (device_event_queue && xQueueReceive(device_event_queue, &event, 0)) {
    type_of_change t = event.t;
    Device *d = event.d;
    
    ESP_LOGD("main", "processing device event %d from task %s", (int)t, pcTaskGetName(xTaskGetCurrentTaskHandle()));

    // a device connecting or going away is worth seeing
    if (t == D_CONNECTED || t == D_DISCONNECTED) {
      TabLock l_lock(lvgl_mutex);
      lv_display_trigger_activity(nullptr);
    }

    bool found = false;
    {
      TabLock lock(tabs_mutex);
      for (const auto& tt : tabs) {
        if (d && tt->device == d) {
          ESP_LOGD("main", "matched an existing tab %s", tt->gettabname());
          tt->last_change = t;
          found = true;
          break;
        }
      }
    }
    
    // a connected device went away: look for it again, in case it comes back. (A failed connection
    // has no tab, and the scan it came from is still running)
    if (found && t == D_DISCONNECTED) ble_start_scan();

    // A tab is only made once the device has connected: a failed connection never gets one, so the
    // scan task can delete that device straight away
    if (found || t != D_CONNECTED) continue;

    ESP_LOGD("main", "a new device has appeared");
    Tab *ta = nullptr;
    switch (d->getType()) {
      case DeviceType::device_mk312:        ta = new tab_mk312(); break;
      case DeviceType::device_zc95:         ta = new tab_zc95(); break;
      case DeviceType::device_coyote:       ta = new tab_coyote(); break;
      case DeviceType::device_funosr:       ta = new tab_funosr(); break;
      case DeviceType::device_lovense:      ta = new tab_lovense(); break;
      case DeviceType::device_breath:       ta = new tab_breath(); break;
      case DeviceType::device_ossm:         ta = new tab_ossm(); break;
      case DeviceType::device_bubblebottle: ta = new tab_bubblebottle(); break;
      case DeviceType::device_dgbutton:     ta = new tab_dgbutton(); break;
      case DeviceType::device_loop:         ta = new tab_loop(); break;
      default:                              continue;
    }
    ta->type = d->getType();
    ta->device = d;
    ta->last_change = t;
    ta->needssetup = true;
    {
      TabLock lock(tabs_mutex);
      tabs.emplace_back(ta);
    }
  }
}

// Handle any tabs that have changed status, this includes
// cleaning up and removing a tab if it's gone away

void handlehardwarecallbacks() {
  TabLock lock(tabs_mutex);
  for (auto st = tabs.begin(); st != tabs.end(); ++ st) {
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
        TabLock l_lock(lvgl_mutex);
        lv_hide_tab(t->page);
        st = tabs.erase(st);
        // Deleting the device also frees its NimBLE client, of which there are only a few
        delete t->device;
        delete t;
        return;
      }
      t->old_last_change = t->last_change;
    }
  }
}

// Call the loop() function for each of the tabs, note if the tab is active (currently visible)

void handletabloops(void) {
  TabLock lock(tabs_mutex);
  TabLock l_lock(lvgl_mutex);
  lv_obj_t *activetab = lv_obj_get_child(lv_tabview_get_content(tv),lv_tabview_get_tab_act(tv));
  for (const auto& t : tabs) {
    t->loop((activetab == t->page));
  }
}

// Dim the screen and knob LEDs once nothing has happened for a while: no touches (which LVGL notes itself), knobs,
// buttons or devices coming and going. The next one brightens it again

constexpr uint32_t dim_after_ms = 5 * 60 * 1000;

void handlescreendimming() {
  static bool dimmed = false;
  bool idle;
  {
    TabLock l_lock(lvgl_mutex);
    idle = lv_display_get_inactive_time(nullptr) > dim_after_ms;
  }
  if (idle != dimmed) {
    ESP_LOGI("main", "screen %s", idle ? "dimmed" : "brightened");
    hardware_set_dimmed(idle);
    pulsemote_pcb_dimleds(idle);
#if CONFIG_HASM5ENCODER
    encoder8_dim_leds(idle);
#endif
    dimmed = idle;
  }
}

// Main UI loop

void main_loop() {
  hardware_tft_loop();
  {
    TabLock l_lock(lvgl_mutex);
    lv_task_handler();
  }
  process_device_events();
  handlehardwarecallbacks();
  handlebuttonpushes();
  handlerotaryencoders();
#if CONFIG_HASM5ENCODER
  handleencoder8();
#endif
  handletabloops();
  handlescreendimming();
  printf_log_flush();
  vTaskDelay(1);
}

// Main loop for UI (FreeRTOS)

void TaskMain(void *pvParameters) {
  vTaskDelay(200);
  ESP_LOGD("main", "Main task started: %s on %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());
  while (true) main_loop();
}

extern "C" void app_main() {
  tabs_mutex = xSemaphoreCreateRecursiveMutex();
  lvgl_mutex = xSemaphoreCreateRecursiveMutex();
  device_event_queue = xQueueCreate(16, sizeof(device_event_t));

  hardware_tft_init();
  ESP_LOGD("setup","display setup done");
  pulsemote_pcb_init();
  ESP_LOGD("setup","pcb setup done");  
  setup_tabs();
  ESP_LOGD("setup","tab setup done");

  printf_log("Version %s\n\n",__DATE__);

  xTaskCreatePinnedToCore(TaskMain, "Main", 1024 * 20, nullptr, 1, nullptr, 1);
  xTaskCreatePinnedToCore(TaskCommsBT, "comms-bt", 1024 * 20, nullptr, 2, nullptr, 0); // ble networking is on core0
}
