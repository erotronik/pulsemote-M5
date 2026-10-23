#pragma once

#include "lvgl-utils.h"
#include "tab.hpp"

class tab_zc95 : public Tab {
 public:
  tab_zc95();
  ~tab_zc95();
  void encoder_change(int sw, int change) override;
  void switch_change(int sw, bool state) override;
  void loop(bool activetab) override;
  int get_levels(level_t levels[levels_max]) override;
  void focus_change(bool focus) override;
  bool hardware_changed(void) override;
  void gotsyncdata(Tab *t, sync_data status) override;
  int wanted_pattern = 0;

  const char *zc95_main_modes_c = "Manual\nTimer\nRandom\nSync";

 private:
  void tab_create(void);
  void zc95_on(void);
  void zc95_off(void);
  void send_levels(void);
  void change_level(int channel, int change);
  int max_level(int channel);
  bool settings_have_focus(void);
  bool ison;
  // Level for each channel (0-99%), set by rotary1-4. rotary3 and rotary4 are also used for the
  // pattern list and settings, after they're clicked
  static const int channels = 4;
  int level[channels] = {0, 0, 0, 0};
  int last_level_button_press[channels] = {0, 0, 0, 0};
};
