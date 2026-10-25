#pragma once

#include "lvgl-utils.h"

class tab_object_segbar {
public:
    tab_object_segbar(lv_obj_t *parent, int w, int h);
    ~tab_object_segbar();

    void set_range(int start_pc, int end_pc);
    void align(lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs);
    void show(bool visible);

private:
    lv_obj_t *base;   // background (usually green)
    lv_obj_t *seg;    // active segment (usually red)
    int width;
    int height;
};
