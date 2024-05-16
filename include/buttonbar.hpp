#pragma once

#include <lvgl.h>
#include <cstdio>
#include "hsv.h"
void m5io_showanalogrgb(byte sw, const CRGB& rgb);

class ButtonBar {
  public:
    ButtonBar(lv_obj_t *parent) {
        container = lv_obj_create(parent);
        // Set the container to be transparent and have no effect
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_outline_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        lv_obj_set_align(container, LV_ALIGN_BOTTOM_LEFT);
        lv_obj_set_width(container, LV_PCT(100));
        lv_obj_set_height(container, LV_SIZE_CONTENT);

        for (int i = 0; i < 5; i++) {
            arc[i] = lv_arc_create(container);
            lv_obj_set_size(arc[i], 60, 60);
            lv_obj_set_align(arc[i], LV_ALIGN_BOTTOM_LEFT);
            lv_obj_set_x(arc[i], (64* i)); //((320-62*5)/4+62)
            lv_arc_set_rotation(arc[i], 270);
            lv_arc_set_bg_angles(arc[i], 0, 360);
            lv_arc_set_value(arc[i], 0);
            lv_obj_remove_style(arc[i], NULL, LV_PART_KNOB);
            lv_obj_remove_flag(arc[i], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_t *xarclabel = lv_label_create(arc[i]);
            lv_label_set_text(xarclabel, "");
            lv_obj_set_style_text_align(xarclabel, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_center(xarclabel);
            //lv_obj_add_event_cb(arc[i], mk312_arc_event_handler, LV_EVENT_ALL, this);
        }
    };
    void settext(int button, const char *text) {
        lv_label_set_text(lv_obj_get_child(arc[button], 0), text);
    }

    void settextfmt(int button, const char *format, ...) {
        static char buf[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        lv_label_set_text(lv_obj_get_child(arc[button], 0), buf);
    }

    void setvalue(int button, int value) {
        lv_arc_set_value(arc[button], value);
    }
    // leds and buttons are numbered differently
    void setrgb(int button, CRGB rgb) {
        static int map[5] = { 2,1,5,3,4};
        m5io_showanalogrgb(map[button], rgb);
    }

  private:
    lv_obj_t *container;
    lv_obj_t *arc[5];
};
