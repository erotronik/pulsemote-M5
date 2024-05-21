#pragma once

#include "tab-object-timer.hpp"
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include "tab.hpp"

class tab_splashscreen: public Tab {
    public:
        tab_splashscreen();
        ~tab_splashscreen();

    void switch_change(int sw, boolean state) override;
    void encoder_change(int sw, int change) override;
    void loop(boolean activetab) override;
    void setup(void) override;
    const char* gettabname(void) override { return "Splashscreen";};

    private:
     unsigned long batterycheckmillis = 0;
     const char *batteryicons[6] = {LV_SYMBOL_BATTERY_EMPTY, LV_SYMBOL_BATTERY_1, LV_SYMBOL_BATTERY_2, LV_SYMBOL_BATTERY_3, LV_SYMBOL_BATTERY_FULL, LV_SYMBOL_CHARGE};
     lv_obj_t *labelicons;
     void updateicons(void);
     byte buttonhue[5] = {0, 0, 0, 0, 0};

};

