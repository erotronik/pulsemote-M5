#pragma once

#include "tab-object-timer.hpp"
#include "lvgl-utils.h"
#include "tab.hpp"

class tab_splashscreen: public Tab {
    public:
        tab_splashscreen();
        ~tab_splashscreen();

    void switch_change(int sw, boolean state) override;
    void encoder_change(int sw, int change) override;
    void loop(boolean activetab) override;
    void setup(void) override;
    const char* gettabname(void) override { return "Pulsemote";};
    void focus_change(boolean focus) override;
    void gotsyncdata(Tab *t, sync_data status) override;
    lv_obj_t *lv_debug_window;

    private:
     unsigned long batterycheckmillis = 0;
     const char *batteryicons[6] = {LV_SYMBOL_BATTERY_EMPTY, LV_SYMBOL_BATTERY_1, LV_SYMBOL_BATTERY_2, LV_SYMBOL_BATTERY_3, LV_SYMBOL_BATTERY_FULL, LV_SYMBOL_CHARGE};
     lv_obj_t *labelicons;
     bool needs_refresh = false;

     void updateicons(void);
     int buttonhue[5] = {0, 0, 0, 0, 0};
     static void popup_add_wifi_device(void);

     struct FoundControl {
      Tab* tab;
      int control;
      char *name;
      bool ison;
      int value;
     };
     static const size_t kMaxFound = 4;
     FoundControl found[kMaxFound];
     size_t found_count = 0;
};

