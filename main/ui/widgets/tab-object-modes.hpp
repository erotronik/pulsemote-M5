#pragma once

#include "lvgl-utils.h"

class tab_object_modes {
  public:
    tab_object_modes();
    void createdropdown(lv_obj_t *parent, const char *options);
    lv_obj_t *getdropdownobject(void) { return dd; }
    bool highlight_next_field(void);
    bool has_focus(void);
    bool rotary_change(int change);
    void reset();

  private:
    lv_obj_t *dd;
};
