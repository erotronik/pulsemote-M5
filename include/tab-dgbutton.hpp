#pragma once

#include "tab.hpp"
#include "device-dgbutton.hpp" 

class tab_dgbutton : public Tab {
 public:
  tab_dgbutton();
  ~tab_dgbutton();

  void loop(boolean activetab) override;
  boolean hardware_changed(void) override;
};
