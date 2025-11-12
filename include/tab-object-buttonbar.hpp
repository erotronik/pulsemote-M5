#pragma once

#include "lvgl-utils.h"
#include <unordered_map>

class tab_object_buttonbar {
  public:
    tab_object_buttonbar(lv_obj_t *parent);

    void set_text(int button, const char *text);
    void set_text_fmt(int button, const char *format, ...);
    void set_click_text(int button, const char *text);
    void set_value(int button, int value);
    void set_rgb(int button, lv_color_t rgb);
    void set_rgb_all(lv_color_t rgb);
    void set_onmain(int button, bool flag);
    void set_ison(int button, bool flag);

    char * get_text(int button);
    int get_value(int button);
    bool get_ison(int button);

    static const int rotary2 = 0;
    static const int rotary1 = 1;
    static const int rotary3 = 2;
    static const int rotary4 = 3;
    static const int switch1 = 4;
    static const int maxbuttons = 5;

    const int rotary_order[maxbuttons] = { rotary1, rotary2, rotary3, rotary4, -1 };

    lv_obj_t *container;

  private:
    lv_obj_t *arc[maxbuttons];
    lv_obj_t *press[maxbuttons];
    const int buttonmaptoposition[maxbuttons] = { 1, 0, 3, 4, 2 };
    const int buttonmaptorgb[maxbuttons] = { 1, 2, 3, 4, 5 };
    bool onmain[maxbuttons] = { false, false, false, false, false };
    bool ison[maxbuttons] = { false, false, false, false, false };
};
