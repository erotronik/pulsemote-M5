#pragma once

#include "tab.hpp"
#include "device-bubblebottle.hpp" 

class tab_bubblebottle : public Tab {
 public:
  tab_bubblebottle();
  ~tab_bubblebottle();

  void loop(boolean activetab) override;
  boolean hardware_changed(void) override;
};
