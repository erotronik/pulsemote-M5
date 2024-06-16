#pragma once

#include "lvgl-utils.h"

class tab_object_modes {
  public:
    tab_object_modes();
    void createdropdown(lv_obj_t *parent, const char *options);
    lv_obj_t *getdropdownobject(void) { return dd; }
    boolean highlight_next_field(void);
    boolean has_focus(void);
    boolean rotary_change(int change);

  private:
    lv_obj_t *dd;
};
