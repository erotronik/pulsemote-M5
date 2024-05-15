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

};

