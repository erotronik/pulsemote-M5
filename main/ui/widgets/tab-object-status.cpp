#include "tab-object-status.hpp"
#include "tab.hpp"
#include <stdio.h>

tab_object_status::tab_object_status(lv_obj_t *parent, int w, int h) {
    container = lv_obj_create(parent);
    lv_obj_add_style(container, &lvpulsemote_style_status, LV_PART_MAIN);
    lv_obj_set_size(container, LV_SCALE(w), LV_SCALE(h));
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);

    label = lv_label_create(container);
    lv_label_set_text(label, "-");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    extra_label = lv_label_create(container);
    lv_label_set_text(extra_label, "");
    lv_obj_align(extra_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_align(extra_label, LV_TEXT_ALIGN_CENTER, 0);

    set_active(false);
}

tab_object_status::~tab_object_status() {}

void tab_object_status::set_active(bool active) {
    is_on = active;
    lv_obj_set_style_bg_color(container, lv_color_hex(is_on ? COLOUR_GREEN : COLOUR_RED), LV_PART_MAIN);
}

void tab_object_status::set_text(const char *text) {
    lv_label_set_text(label, text);
}

void tab_object_status::set_text_fmt(const char *fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    lv_label_set_text(label, buf);
}

void tab_object_status::set_extra_text(const char *text) {
    lv_label_set_text(extra_label, text);
}

void tab_object_status::align(lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs) {
    lv_obj_align(container, align, x_ofs, y_ofs);
}

void tab_object_status::show(bool visible) {
    if (visible) {
        lv_obj_remove_flag(container, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
    }
}
