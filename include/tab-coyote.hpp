#pragma once

#include "tab-object-timer.hpp"
#include "tab-object-sync.hpp"
#include "tab-object-modes.hpp"
#include "lvgl-utils.h"
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
        static void coyote_mode_change_cb(lv_event_t *event);

        void coyote_tab_create(void);
        lv_obj_t *tab_status;
        tab_object_modes *modeselect;
        tab_object_sync *sync;
        tab_object_timer *rand_timer;
        tab_object_timer *timer;
        void tab_create_status(lv_obj_t *tv2);
        bool ison;
        coyote_mode mode_a;
        coyote_mode mode_b;
        int timermillis = 0;
};

