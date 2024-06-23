#pragma once

#include "lvgl-utils.h"
#include <unordered_map>

class tab_object_buttonbar {
  public:
    tab_object_buttonbar(lv_obj_t *parent);

    void set_text(int button, const char *text);
    void set_text_fmt(int button, const char *format, ...);
    void set_value(int button, int value);
    void set_rgb(int button, lv_color_t rgb);
    void set_rgb_all(lv_color_t rgb);

    static const int rotary2 = 0;
    static const int rotary1 = 1;
    static const int rotary3 = 2;
    static const int rotary4 = 3;
    static const int switch1 = 4;
    static const int maxbuttons = 5;

  private:
    lv_obj_t *container;
    lv_obj_t *arc[maxbuttons];
    const int buttonmaptoposition[5] = { 1, 0, 3, 4, 2 };
    const int buttonmaptorgb[5] = { 1, 2, 3, 4, 5 };
};
