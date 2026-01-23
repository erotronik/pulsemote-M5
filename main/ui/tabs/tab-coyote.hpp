#pragma once

#include "lvgl-utils.h"
#include "tab.hpp"
#include "device-coyote.hpp"

class tab_coyote: public Tab {
    public:
        tab_coyote();
        ~tab_coyote();
        void encoder_change(int sw, int change) override;
        virtual bool hardware_changed(void) override;
        void switch_change(int sw, bool state) override;
        void loop(bool activetab) override;
        void focus_change(bool focus) override;
        void gotsyncdata(Tab *t, sync_data status) override;
        const char *coyote_main_modes_c =  "Manual\nTimer\nRandom\nSync";  

    private:
        static void coyote_mode_change_cb(lv_event_t *event);
        void coyote_tab_create(void);
        time_t last_refresh = 0;
        bool ison;
        coyote_mode mode_a;
        coyote_mode mode_b;
        int level_a_req;
        int level_b_req;  
};

