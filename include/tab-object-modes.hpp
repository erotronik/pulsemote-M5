#pragma once

#include "lvgl-utils.h"

class tab_object_modes {
  public:
    tab_object_modes();
    void createdropdown(lv_obj_t *parent, const char *options);
    lv_obj_t *getdropdownobject(void) { return dd; }
    boolean handleclick(void);
    boolean handleencoder(int change);

  private:
    lv_obj_t *dd;
    boolean hasfocus = false;
};
