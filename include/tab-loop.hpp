#pragma once

#include "tab.hpp"
#include "device-loop.hpp" 

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
};
