#pragma once

#include "tab.hpp"
#include "device-dgbutton.hpp" 

class tab_dgbutton : public Tab {
 public:
  tab_dgbutton();
  ~tab_dgbutton();

  void loop(bool activetab) override;
  bool hardware_changed(void) override;

 private:
  bool held = false;
  lv_obj_t *battery_icon = nullptr;
  int battery_shown = 0;
  void update_battery_icon(void);
};
