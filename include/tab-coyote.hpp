#pragma once

#include "tab-object-timer.hpp"
#include "tab-object-sync.hpp"
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include "tab.hpp"
#include "device-coyote2.hpp"

class tab_coyote: public Tab {
    public:
        tab_coyote();
        ~tab_coyote();
        void encoder_change(int sw, int change) override;
        virtual boolean hardware_changed(void) override;
        void switch_change(int sw, boolean state) override;
        void loop(boolean activetab) override;
        void focus_change(boolean focus) override;
        tab_object_timer *rand_timer;
        tab_object_timer *timer;
        tab_object_sync *sync;
        void gotsyncdata(Tab *t, sync_data status) override;

        enum main_modes {
            MODE_MANUAL = 0,
            MODE_TIMER,
            MODE_RANDOM,
            MODE_SYNC,
        };
        const char *coyote_main_modes_c =  "Manual\nTimer\nRandom\nSync";  

        main_modes main_mode;
        bool need_refresh;

    private:
        void coyote_tab_create(void);
        lv_obj_t *tab_status;
        void tab_create_status(lv_obj_t *tv2);
        bool ison;
        coyote_mode mode_a;
        coyote_mode mode_b;
        int timermillis = 0;
};

