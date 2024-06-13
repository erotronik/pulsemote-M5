#pragma once

#include "lvgl-utils.h"

class tab_object_buttonbar {
  public:
    tab_object_buttonbar(lv_obj_t *parent);
    void settext(int button, const char *text);
    void settextfmt(int button, const char *format, ...);
    void setvalue(int button, int value);
    void setrgb(int button, lv_color_t rgb);
    static const int maxbuttons = 5;

  private:
    lv_obj_t *container;
    lv_obj_t *arc[maxbuttons];
};
