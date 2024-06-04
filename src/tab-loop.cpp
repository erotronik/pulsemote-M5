#include "tab.hpp"
#include "lvgl-utils.h" // for printf_log()
#include "device-loop.hpp" 
#include "tab-loop.hpp"
#include <lvgl.h>
#include "lvgl-utils.h"

tab_loop::tab_loop() {
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
}

tab_loop::~tab_loop() {
}

void tab_loop::loop(boolean activetab) {
  device_loop *md = static_cast<device_loop *>(device);
  if (md && ser1) {
    int x = md->get_reading();
    ser1->y_points[0] = x%100;
  }
  if (activetab) {
    lv_chart_refresh(chart);
  }
}

void tab_loop::loop_tab_create() {
  device_loop* md = static_cast<device_loop*>(device);
  lv_obj_t *tv1 = lv_tabview_add_tab(tv, md->getShortName());

  lv_obj_set_style_pad_left(tv1, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_right(tv1,0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(tv1, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(tv1, 0, LV_PART_MAIN);

  chart = lv_chart_create(tv1);
  lv_chart_set_type(chart,LV_CHART_TYPE_LINE);
  ser1 = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
  lv_obj_set_size(chart, LV_PCT(100), 100);
  lv_chart_set_div_line_count(chart, 10, 90);


  uint32_t i;
  for(i = 0; i < 240; i++) {
    lv_chart_set_next_value(chart, ser1, lv_rand(10, 50));
  }
  lv_chart_refresh(chart); /*Required after direct set*/
  
  buttonbar = new tab_object_buttonbar(tv1);
  int tabid = lv_get_tabview_idx_from_page(tv, tv1);
  page = tv1;
  lv_tabview_set_act(tv,tabid, LV_ANIM_OFF);
}
  
// return false if we removed ourselves from the connected devices list
boolean tab_loop::hardware_changed(void) {
  if (last_change == D_CONNECTING) {
    printf_log("Connecting %s\n", device->getShortName());
  } else if (last_change == D_CONNECTED) {
    device_loop *md = static_cast<device_loop *>(device);
    send_sync_data(SYNC_START);
    send_sync_data(SYNC_OFF);
    xQueueReset(md->events);
    printf_log("Connected %s\n", device->getShortName());
    loop_tab_create();
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", device->getShortName());
    send_sync_data(SYNC_BYE);
    return false;
  }
  return true;
}
