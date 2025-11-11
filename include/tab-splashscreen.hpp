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
    lv_obj_t *lv_debug_window;


    private:
     unsigned long batterycheckmillis = 0;
     const char *batteryicons[6] = {LV_SYMBOL_BATTERY_EMPTY, LV_SYMBOL_BATTERY_1, LV_SYMBOL_BATTERY_2, LV_SYMBOL_BATTERY_3, LV_SYMBOL_BATTERY_FULL, LV_SYMBOL_CHARGE};
     lv_obj_t *labelicons;
     bool needs_refresh = false;

     void updateicons(void);
     int buttonhue[5] = {0, 0, 0, 0, 0};
     void dump_connected_devices(void);
     static void popup_add_wifi_device(void);

};

