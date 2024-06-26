#pragma once

#include "tab.hpp"
#include "device-loop.hpp" 

class tab_loop : public Tab {
  public:
    tab_loop();
    ~tab_loop();

    void loop(boolean activetab) override;
    boolean hardware_changed(void) override;
    void encoder_change(int sw, int change) override;
    void switch_change(int sw, boolean state) override;
    void focus_change(boolean focus) override;

  private:
    void loop_tab_create();
    void tab_create_status(void);
    void update_chart(int32_t new_data);
    void changed_state(void);

    static const int LOOP_DATA_POINTS = 150;

    lv_obj_t *tab_status;
    lv_chart_series_t *graph_data = NULL;
    lv_obj_t *chart = NULL;
    lv_chart_series_t *line_max;
    lv_chart_series_t *line_min;
    lv_chart_series_t *line_current;
    int setpoint_min =0;
    int setpoint_max =0;
    bool is_on = false;

    int32_t data[LOOP_DATA_POINTS] = {0}; 
};
