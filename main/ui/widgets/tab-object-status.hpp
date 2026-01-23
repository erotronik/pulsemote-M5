#pragma once

#include "lvgl-utils.h"
#include <stdarg.h>

class tab_object_status {
public:
    tab_object_status(lv_obj_t *parent, int w = 144, int h = 64);
    ~tab_object_status();

    void set_active(bool is_on);
    void set_text(const char *text);
    void set_text_fmt(const char *fmt, ...);
    void set_extra_text(const char *text);
    void align(lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs);
    void show(bool visible);

private:
    lv_obj_t *container;
    lv_obj_t *label;
    lv_obj_t *extra_label;
    bool is_on = false;
};
