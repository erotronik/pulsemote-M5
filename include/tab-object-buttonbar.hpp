#pragma once

#include <lvgl.h>
#include "lvgl-utils.h"

class tab_object_buttonbar {
  public:
    tab_object_buttonbar(lv_obj_t *parent);
    void settext(int button, const char *text);
    void settextfmt(int button, const char *format, ...);
    void setvalue(int button, int value);
    void setrgb(int button, CRGB rgb);

  private:
    lv_obj_t *container;
    lv_obj_t *arc[5];
};
