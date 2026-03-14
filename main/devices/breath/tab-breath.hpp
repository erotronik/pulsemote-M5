#pragma once

#include "lvgl-utils.h"
#include "tab.hpp"
#include "tab-object-segbar.hpp"

class tab_breath : public Tab {
 public:
  tab_breath();
  ~tab_breath();
  void encoder_change(int sw, int change) override;
  void switch_change(int sw, bool state) override;
  void loop(bool activetab) override;
  void focus_change(bool focus) override;
  bool hardware_changed(void) override;
  void gotsyncdata(Tab *t, sync_data status) override;

  const char *breath_main_modes_c = "Manual\nTimer\nRandom";

 private:
  static constexpr unsigned long KEEPALIVE_MS   = 1000;
  static constexpr unsigned long MAX_RUNTIME_MS  = 90*1000;
  static constexpr unsigned long RECOVERY_MS     = 5*1000;

  int main_pattern = 0;
  static void breath_mode_change_cb(lv_event_t *event);
  void tab_create(void);
  bool need_knob_refresh = false;
  bool timed_cycle_active = false;
  void set_breath_block(int pc);
  void send_breath_block();
  unsigned long last_block_start = 0;
  unsigned long last_block_sent = 0;
  unsigned long last_block_end = 0;
  unsigned long manual_on_until = 0;
  int last_manual_countdown = 0;
  int breath_block = 0;
  tab_object_timer *manual_timer = nullptr;

  enum pattern_mode { P_RESTRICT, P_BLOCK, P_RANDOM };
  pattern_mode current_pattern_mode;
  static constexpr int NUMBER_OF_PATTERN_MODES = 2; // ignore P_RANDOM for now.

  const char* pattern_str(pattern_mode i) {
    if (i == P_BLOCK) return "Block";
    if (i == P_RESTRICT)  return "Restrict";
    if (i == P_RANDOM) return "Random";
    return "";
  };

  int get_breath_pc(pattern_mode mode) {
    if (mode == P_BLOCK) return 100;
    if (mode == P_RESTRICT) return 50;
    if (mode == P_RANDOM) return (rand() % 2) * 50 + 50;
    return 0;
  };
};
