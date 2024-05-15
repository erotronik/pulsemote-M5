#include <LinkedList.h>
#include <M5Unified.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <memory>

#include "comms-bt.hpp"

#define LV_CONF_INCLUDE_SIMPLE
#include <esp_timer.h>
#include <lvgl.h>

#include "m5io.h"
#include "tab-coyote.hpp"
#include "tab-mk312.hpp"
#include "tab-splashscreen.hpp"
#include "tab.hpp"

lv_obj_t *tv;

LinkedList<Tab *> tabs = LinkedList<Tab *>();

constexpr int32_t SCREENW = 320;
constexpr int32_t SCREENH = 240;

lv_display_t *display;
lv_indev_t *indev;

void lvgl_display_flush(lv_display_t *disp, const lv_area_t *area,
                      uint8_t *px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  lv_draw_sw_rgb565_swap(px_map, w * h);
  M5.Display.pushImageDMA<uint16_t>(area->x1, area->y1, w, h,
                                    (uint16_t *)px_map);
  lv_disp_flush_ready(disp);
}

uint32_t lvgl_tick_function() { return (esp_timer_get_time() / 1000LL); }

void lvgl_touchpad_read(lv_indev_t *drv, lv_indev_data_t *data) {
  M5.update();
  auto count = M5.Touch.getCount();

  if (count == 0) {
    data->state = LV_INDEV_STATE_RELEASED;
  } else {
    auto touch = M5.Touch.getDetail(0);
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = touch.x;
    data->point.y = touch.y;
  }
}

byte lastencodertest[numencoders] = {128, 128, 128, 128};
byte encodertest[numencoders] = {128, 128, 128, 128};

void RotaryEncoderChanged(bool clockwise, int id) {
  if (clockwise) {
    encodertest[id]++;
  } else {
    encodertest[id]--;
  }
}

// just a test mode to display what we think all the connected
// devices are to the debug window when you push a button
// on the splashscreen

void dump_connected_devices(void) {
  for (int i = 0; i < tabs.size(); i++) {
    Tab *t = tabs.get(i);
    printf_log("tab %d: ", i);
    if (t->page != nullptr) {
      printf_log("tab=%d ", lv_get_tabview_idx_from_page(tv, t->page));
    }
    if (t->device != nullptr) {
      printf_log("device=%s", t->device->getShortName());
    }
    printf_log("\n");
  }
}

// called from main, look to see if we've got any button pushes
// from the interrupt queue and dispatch them to the callback of
// the device with current open tab

void handlebuttonpushes() {
  event_t received_event;
  byte count = 4;  // a few callbacks allowed per loop
  while (count > 0 && xQueueReceive(event_queue, &received_event, 0)) {
    // find what device tab is active as physical buttons must only work on
    // active tab
    lv_obj_t *activepage = lv_obj_get_child(lv_tabview_get_content(tv),lv_tabview_get_tab_act(tv));
    for (int j = 0; j < tabs.size(); j++) {
      Tab *t = tabs.get(j);
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
    int change = encodertest[i] - lastencodertest[i];
    if (change != 0) {
      lastencodertest[i] = encodertest[i] = 128;
      // find what device tab is active as encoders must only work on active tab
      lv_obj_t *activepage = lv_obj_get_child(lv_tabview_get_content(tv),lv_tabview_get_tab_act(tv));
      for (int j = 0; j < tabs.size(); j++) {
        Tab *t = tabs.get(j);
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
  ESP_LOGD("main", "tabview cb %s on %d: current tab %d\n",
           pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID(),
           lv_tabview_get_tab_act(tv));
  lv_obj_t *activepage = lv_obj_get_child(lv_tabview_get_content(tv),lv_tabview_get_tab_act(tv));
  for (int j = 0; j < tabs.size(); j++) {
    Tab *t = tabs.get(j);
    if (activepage == t->page)
      t->focus_change(true);  // true means is the new active tab
  }
}

// set up the tab view and create the first default tab, the
// welcome screen (splashscreen)

void setup_tabs(void) {
  tv = lv_tabview_create(lv_screen_active());
  lv_obj_set_style_text_font(lv_tabview_get_tab_bar(tv), &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_add_event_cb(tv, tabview_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_tabview_set_tab_bar_size(tv, 34);

  Tab *sp = new tab_splashscreen();
  sp->setup();
  tabs.add(sp);
}

void device_change_handler(type_of_change t, Device *d) {
  ESP_LOGD("main", "change handler task called from %s on %d\n",
           pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

  int newdevice = -1;
  for (int i = 0; i < tabs.size(); i++) {
    Tab *tt = tabs.get(i);
    if (tt->device == d) {
      tt->last_change = t;
      newdevice = i;
      break;
    }
  }
  if (newdevice == -1 && t != D_DISCONNECTED &&
      t != static_cast<type_of_change>(C_DISCONNECTED)) {
    ESP_LOGD("main", "a new device has appeared");
    DeviceType type = d->getType();
    if (type == DeviceType::device_mk312) {  // bootstrap
      Tab *ta = new tab_mk312();
      ta->type = type;
      ta->device = d;
      ta->last_change = t;
      ta->setup();
      tabs.add(ta);
    } else if (type == DeviceType::device_coyote2) {
      Tab *ta = new tab_coyote();
      ta->type = type;
      ta->device = d;
      ta->last_change = t;
      ta->setup();
      tabs.add(ta);
    }
  }
}

void main_loop(void);
void TaskMain(void *pvParameters) {
  vTaskDelay(200);
  ESP_LOGD("main", "Main task started: %s on %d",
           pcTaskGetName(xTaskGetCurrentTaskHandle()), xPortGetCoreID());

  while (true) main_loop();
}

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

  xTaskCreatePinnedToCore(TaskMain, "Main", 1024 * 10, nullptr, 1, nullptr, 1);
  xTaskCreatePinnedToCore(TaskCommsBT, "comms-bt", 1024 * 12, nullptr, 2,
                          nullptr, 0);
}

void loop() {};

void lv_hide_tab(lv_obj_t *page) {
  if (page == nullptr) return;
  int tabid = lv_get_tabview_idx_from_page(tv, page);
  ESP_LOGD("main", "hide tab id %d", tabid);
  if (tabid == -1) return;
  // if we're viewing the tab that's gone away then switch to the main screen
  if (lv_tabview_get_tab_act(tv) == tabid)
    lv_tabview_set_act(tv, 0, LV_ANIM_OFF);

  lv_obj_t *tbar = lv_tabview_get_tab_bar(
      tv);  // in lvgl 9 they are real buttons not a matrix
  lv_obj_t *cont = lv_tabview_get_content(tv);
  lv_obj_del(lv_obj_get_child(tbar, tabid));
  lv_obj_del(lv_obj_get_child(cont, tabid));
}

void handlehardwarecallbacks() {
  for (int i = 0; i < tabs.size(); i++) {
    Tab *t = tabs.get(i);
    if (t->old_last_change != t->last_change) {
      ESP_LOGD("main", "%s (%d) changed state: %d %d\n",
               t->device->getShortName(), i, (int)t->last_change,
               (int)t->old_last_change);
      if (!t->hardware_changed()) {
        // false means the device has gone away, get rid of the tab
        lv_hide_tab(t->page);
        tabs.remove(i);
        break;  // any other tab changes pick up next time
      }
      t->old_last_change = t->last_change;
    }
  }
}

void handletabloops(void) {
  lv_obj_t *activetab = lv_obj_get_child(lv_tabview_get_content(tv),lv_tabview_get_tab_act(tv));
  for (int i = 0; i < tabs.size(); i++) {
    Tab *t = tabs.get(i);
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

void printf_log(const char *format, ...) {
  static char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, 255, format, args);
  va_end(args);
  Serial.print(buf);
  if (tabs.size() > 0) {
    Tab *t = tabs.get(0);
    if (t->page) {
      lv_obj_t *child = lv_obj_get_child(t->page, 0);
      lv_textarea_add_text(child, buf);
    }
  }
}
