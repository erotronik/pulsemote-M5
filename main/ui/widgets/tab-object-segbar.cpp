#include "tab-object-segbar.hpp"
#include "tab.hpp"
#include <algorithm>

tab_object_segbar::tab_object_segbar(lv_obj_t *parent, int w, int h) {
    width = w;
    height = h;

    // background base
    base = lv_obj_create(parent);
    lv_obj_remove_style_all(base);                  
    lv_obj_set_size(base, width, height);
    lv_obj_set_style_bg_color(base, lv_color_hex(COLOUR_GREEN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(base, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(base, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(base, 0, LV_PART_MAIN);
    lv_obj_remove_flag(base, LV_OBJ_FLAG_SCROLLABLE);

    // active segment (child)
    seg = lv_obj_create(base);
    lv_obj_remove_style_all(seg);
    lv_obj_set_style_bg_color(seg, lv_color_hex(COLOUR_RED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(seg, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(seg, 0, LV_PART_MAIN);
    lv_obj_remove_flag(seg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(seg, LV_OBJ_FLAG_FLOATING);        // stay on top / ignore layouts

    set_range(0, 0);
}

tab_object_segbar::~tab_object_segbar() {}

void tab_object_segbar::set_range(int start_pc, int end_pc) {
    int xs = std::max(0, std::min(100, start_pc)) * width / 100;
    int ys = std::max(0, std::min(100, end_pc)) * width / 100;

    if (ys < xs) std::swap(xs, ys);

    int w = ys - xs;
    lv_obj_set_pos(seg, xs, 0);
    lv_obj_set_size(seg, std::max(0, w), height);
}

void tab_object_segbar::align(lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs) {
    lv_obj_align(base, align, x_ofs, y_ofs);
}

void tab_object_segbar::show(bool visible) {
    if (visible) {
        lv_obj_remove_flag(base, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(base, LV_OBJ_FLAG_HIDDEN);
    }
}
