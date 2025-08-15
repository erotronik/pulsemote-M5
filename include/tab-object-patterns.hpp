#pragma once

#include "lvgl-utils.h"

class tab_object_patterns {
  public:
    tab_object_patterns();
    void selectpattern(lv_obj_t *parent, const char *options[], int n);
    lv_obj_t *getdropdownobject(void) { return dd; }
    boolean highlight_next_field(void);
    boolean visible(void);
    uint32_t hide(void);
    void show(int i=0);

    boolean rotary_change(int change);
    void reset();

  private:
    bool is_visible = false;
    lv_obj_t *dd;
    void pattern_change_cb(lv_event_t *event);

};
