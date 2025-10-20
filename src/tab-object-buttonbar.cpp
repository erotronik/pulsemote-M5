#include "lvgl-utils.h"
#include "tab-object-buttonbar.hpp"

// Pushed the screen on an arc?

#if 0
void mk312_arc_event_handler(lv_event_t *event) {
  tab_mk312 *mk312_tab =
      static_cast<tab_mk312 *>(lv_event_get_user_data(event));
  lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_CLICKED) {
  ESP_LOGD("mk312","touched an arc");
    for (int i=0;i<5;i++) {
      if ((lv_obj_t *)lv_event_get_target(event) == mk312_tab->arc[i]) {
          ESP_LOGD("mk312","touched arc %d",i);
          mk312_tab->switch_change(i, true);
      }
    }
  }
}
#endif

tab_object_buttonbar::tab_object_buttonbar(lv_obj_t *parent) {
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

  lv_obj_set_style_pad_top(container, 25, 0);     // <-- add headroom

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

    // --- Badge ABOVE the arc (compact, won't cover the circle) ---
    press[i] = lv_obj_create(container);
    lv_obj_add_flag(press[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_style_all(press[i]);                           // start clean
    lv_obj_set_size(press[i], LV_SIZE_CONTENT, LV_SIZE_CONTENT); // <- key: autosize to content
    lv_obj_clear_flag(press[i], LV_OBJ_FLAG_SCROLLABLE);         // no internal scroll
    lv_obj_set_style_radius(press[i], 8, 0);
    lv_obj_set_style_bg_opa(press[i], LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(press[i], lv_palette_darken(LV_PALETTE_BLUE, 3), 0);
    lv_obj_set_style_pad_hor(press[i], 6, 0);
    lv_obj_set_width(press[i],60);
    lv_obj_set_style_pad_ver(press[i], 2, 0);
    lv_obj_set_style_border_width(press[i], 0, 0);

    // Label inside the badge
    lv_obj_t *badge_label = lv_label_create(press[i]);
    lv_label_set_text(badge_label, "");
    lv_label_set_long_mode(badge_label, LV_LABEL_LONG_CLIP);  // don't wrap "te te te ..."
    lv_obj_set_style_text_color(badge_label, lv_color_white(), 0);
    lv_obj_center(badge_label);

    // Place the badge just above the arc
    lv_obj_align_to(press[i], arc[i], LV_ALIGN_OUT_TOP_MID, 0, -6);

    //lv_obj_add_event_cb(arc[i], mk312_arc_event_handler, LV_EVENT_ALL, this);
  }
}

void tab_object_buttonbar::set_click_text(int button, const char *text) {
  if (text == "" ) {
  lv_obj_add_flag(press[buttonmaptoposition[button]], LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(press[buttonmaptoposition[button]], LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lv_obj_get_child(press[buttonmaptoposition[button]], 0), text);
  }
}

void tab_object_buttonbar::set_text(int button, const char *text) {
  lv_label_set_text(lv_obj_get_child(arc[buttonmaptoposition[button]], 0), text);
}

void tab_object_buttonbar::set_text_fmt(int button, const char *format, ...) {
  static char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  lv_label_set_text(lv_obj_get_child(arc[buttonmaptoposition[button]], 0), buf);
}

void tab_object_buttonbar::set_value(int button, int value) {
  lv_arc_set_value(arc[buttonmaptoposition[button]], value);
}
   
// leds and buttons are numbered differently
void tab_object_buttonbar::set_rgb(int button, lv_color_t rgb) {
  m5io_showanalogrgb(buttonmaptorgb[button], rgb);
}

void tab_object_buttonbar::set_rgb_all(lv_color_t rgb) {
  for (int button : {tab_object_buttonbar::rotary1, tab_object_buttonbar::rotary2, tab_object_buttonbar::rotary3, tab_object_buttonbar::rotary4, tab_object_buttonbar::switch1}) {
    m5io_showanalogrgb(buttonmaptorgb[button], rgb);
  }
}