#pragma once

#include "tab.hpp"
#include "device-loop.hpp" 

#define LOOP_DATA_POINTS 150

class tab_loop : public Tab {
 public:
  tab_loop();
  ~tab_loop();

  void loop(boolean activetab) override;
  boolean hardware_changed(void) override;

 private:
   void loop_tab_create();
   lv_chart_series_t * ser1 = NULL;
  lv_obj_t *chart = NULL;
  lv_chart_series_t *line_max;
  lv_chart_series_t *line_min;
  lv_chart_series_t *line_current;
  int setpoint_min =0;
  int setpoint_max =0;
  bool is_on = false;

  void update_chart(int32_t new_data);
  int32_t data[LOOP_DATA_POINTS] = {0}; 
};
