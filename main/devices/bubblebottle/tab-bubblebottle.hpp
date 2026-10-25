#pragma once

#include "tab.hpp"
#include "device-bubblebottle.hpp" 

class tab_bubblebottle : public Tab {
 public:
  tab_bubblebottle();
  ~tab_bubblebottle();

  void loop(bool activetab) override;
  bool hardware_changed(void) override;
  const char *activity_name(void) override { return "Breathing in"; }
  bool get_activity(void) override { return breath_in; }
  const char *rate_name(void) override { return "Breaths per minute"; }
  float get_rate(void) override;

 private:
  lv_obj_t *icon = nullptr;
  bool breath_in = false;
  // When the last few breaths in started, oldest first
  static const int breaths_max = 6;
  unsigned long breaths[breaths_max];
  int breath_count = 0;
  void add_breath(void);
  void update_icon(bool breath_in);
};
