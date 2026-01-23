#include "tab-object-status.hpp"
#include "tab.hpp"
#include <stdio.h>
#include <string>
#include <algorithm>

static void set_text_and_toggle(lv_obj_t* label, const char* text) {
    if (!text || text[0] == '\0' || (text[0] == '-' && text[1] == '\0')) {
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label, "");
    } else {
        lv_obj_remove_flag(label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label, text);
    }
}

tab_object_status::tab_object_status(lv_obj_t *parent, int w, int h) {
    container = lv_obj_create(parent);
    lv_obj_add_style(container, &lvpulsemote_style_status, LV_PART_MAIN);
    // Use fixed width but dynamic height, with a minimum height for 2 lines
    lv_obj_set_size(container, LV_SCALE(w), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(container, LV_SCALE(60), 0);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);
    
    // Stack labels vertically using Flex
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(container, LV_SCALE(2), 0);
    lv_obj_set_style_pad_all(container, LV_SCALE(4), 0);

    label = lv_label_create(container);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(label, LV_PCT(100));
    set_text_and_toggle(label, "-");

    extra_label = lv_label_create(container);
    lv_obj_set_style_text_align(extra_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(extra_label, LV_PCT(100));
    set_text_and_toggle(extra_label, "");

    set_active(false);
}

tab_object_status::~tab_object_status() {}

void tab_object_status::set_active(bool active) {
    is_on = active;
    lv_obj_set_style_bg_color(container, lv_color_hex(is_on ? COLOUR_GREEN : COLOUR_RED), LV_PART_MAIN);
}

void tab_object_status::set_text(const char *text) {
    set_text_and_toggle(label, text);
}

void tab_object_status::set_text_fmt(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    set_text_and_toggle(label, buf);
}

void tab_object_status::set_extra_text(const char *text) {
    set_text_and_toggle(extra_label, text);
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
