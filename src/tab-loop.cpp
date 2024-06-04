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

void tab_loop::update_chart(int32_t new_data) {
    static long shifttime = millis()+2000; // let data settle a bit
    bool doshift = false;
    if (millis() > shifttime) {

      if (data[0] == 0 && ser1) {
        for(int i = 0; i < LOOP_DATA_POINTS; i++) {
          data[i] = new_data;
        }
        setpoint_min = new_data -5;
        setpoint_max = new_data +5;
        lv_chart_set_all_value(chart, line_min, setpoint_min);
        lv_chart_set_all_value(chart, line_max, setpoint_max);

      }
      shifttime = millis()+1500;  // 150 points we want say 3 mins
      doshift = true;
    }
    // Shift existing data to the left sometimes
    int32_t loopreadingmin = 1024;
    int32_t loopreadingmax = 0;
    for (int i = 0; i < LOOP_DATA_POINTS - 1; i++) {
        if (doshift) data[i] = data[i + 1];
        loopreadingmin = min(data[i], loopreadingmin);
        loopreadingmax = max(data[i], loopreadingmax);        
    }
    if (ser1) {
      lv_chart_set_all_value(chart, line_current, new_data);
    }
    // Add new data at the end
    data[LOOP_DATA_POINTS - 1] = new_data;
    loopreadingmin = min(new_data, loopreadingmin);
    loopreadingmax = max(new_data, loopreadingmax);       
    loopreadingmin = min(setpoint_min, loopreadingmin);
    loopreadingmax = max(setpoint_max, loopreadingmax);  
    // Update chart
    if (ser1) {
      lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, loopreadingmin, loopreadingmax);
    }
}


void tab_loop::loop(boolean activetab) {
  int state;
  device_loop *md = static_cast<device_loop *>(device);
  if (md && xQueueReceive(md->events,&state, 0)) {
    if (state > setpoint_max && is_on) {
      is_on = false;
      send_sync_data(SYNC_OFF);
    } else if (state < setpoint_min && is_off) {
      is_on = true;
      send_sync_data(SYNC_ON);
    }
    update_chart(state);
    if (activetab && ser1) {
      lv_chart_refresh(chart);
    }
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
  lv_chart_set_point_count(chart, LOOP_DATA_POINTS);
  ser1 = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
  lv_obj_set_size(chart, 240, 100);
  lv_chart_set_div_line_count(chart, 10, 90);
  lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_CIRCULAR);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1024); // will autoscale later

  lv_chart_set_ext_y_array(chart, ser1, data);
  lv_obj_set_style_size(chart, 1, 1, LV_PART_INDICATOR); // no circles
  
  line_max = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
  line_min = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
  line_current = lv_chart_add_series(chart, lv_color_hex(0x005500), LV_CHART_AXIS_PRIMARY_Y);

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
