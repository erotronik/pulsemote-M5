#include "lvgl-utils.h"
#include "tab-object-buttonbar.hpp"
#include "tab.hpp"
#include <pulsemote-pcb.hpp>
#include <cmath>

// We don't want the default handler for the arcs as you can jump to
// 100 without much effort, instead follow clicks around the arc

//static float wrap_deg(float a) {
//    a = std::fmod(a, 360.0f);
//    if (a < 0.0f) a += 360.0f;
//    return a;
//}

void tab_object_buttonbar::arc_event_cb(lv_event_t * e) {
    tab_object_buttonbar * self = static_cast<tab_object_buttonbar *>(lv_event_get_user_data(e));
    lv_obj_t* hit = static_cast<lv_obj_t *>(lv_event_get_target(e));
    lv_obj_t* obj = lv_obj_get_parent(hit);

    // pushed the arc or the button above it?
    int arc_index = -1;
    for (int i = 0; i < maxbuttons; ++i) {
        if (self->arc[i] == obj) { arc_index = i; break; }
    }
    if (arc_index < 0) {
      for (int i = 0; i < maxbuttons; ++i) {
        if (self->press[i] == hit) { 
          for (const auto& t : tabs) {
            if (self == t->buttonbar) t->switch_change(t->buttonbar->all_order[i], true);
          }
        }
      }
      return;
    }

    lv_indev_t * indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);

    static time_t lastclickmillis = 0;
    static int increment = 1;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    lv_coord_t cx = coords.x1 + lv_area_get_width(&coords)  / 2;
    //lv_coord_t cy = coords.y1 + lv_area_get_height(&coords) / 2;

    float dx = (float)p.x - (float)cx;
    //float dy = (float)p.y - (float)cy;

    time_t now = millis();
    int ms = now-lastclickmillis;
    lastclickmillis = now;
    increment = (ms<500)?5:1;

    // find the tab....
    for (const auto& t : tabs) {
      if (self == t->buttonbar) {
        int contr = t->buttonbar->all_order[arc_index];
        ESP_LOGD("click","found tab %s",t->gettabname());
        if (contr == tab_object_buttonbar::switch1)
          t->switch_change(contr, true);
        else
          t->encoder_change(contr, dx>0?increment:-increment);
      }
    }
}

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
  lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

  // Enable Flex layout for equal spacing
  lv_obj_set_layout(container, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);

  const int arc_size = LV_SCALE(60);
  const int arc_label_gap = LV_SCALE(6);
  const int wrapper_h = arc_size + LV_SCALE(25) + arc_label_gap;
  lv_obj_set_style_pad_top(container, arc_label_gap, 0); 

  for (int i = 0; i < maxbuttons; i++) {
    // Wrapper provides a hit-testable area for both badge and arc in a stable layout
    lv_obj_t *wrapper = lv_obj_create(container);
    lv_obj_remove_style_all(wrapper);
    lv_obj_set_size(wrapper, arc_size, wrapper_h);
    lv_obj_clear_flag(wrapper, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(wrapper, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(wrapper, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(wrapper, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(wrapper, arc_label_gap, 0);

    // --- Badge ABOVE the arc ---
    press[i] = lv_obj_create(wrapper);
    lv_obj_add_flag(press[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_style_all(press[i]);
    lv_obj_set_size(press[i], arc_size, LV_SIZE_CONTENT);
    lv_obj_clear_flag(press[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(press[i], LV_SCALE(8), 0);
    lv_obj_set_style_bg_opa(press[i], LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(press[i], lv_palette_darken(LV_PALETTE_BLUE, 3), 0);
    lv_obj_set_style_pad_hor(press[i], LV_SCALE(4), 0);
    lv_obj_set_style_pad_ver(press[i], LV_SCALE(2), 0);
    lv_obj_set_style_border_width(press[i], 0, 0);
    lv_obj_add_event_cb(press[i], arc_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(press[i], LV_OBJ_FLAG_CLICKABLE);

    // Label inside the badge
    lv_obj_t *badge_label = lv_label_create(press[i]);
    lv_label_set_text(badge_label, "");
    lv_label_set_long_mode(badge_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(badge_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(badge_label, LV_FONT_GET(14), 0);
    lv_obj_center(badge_label);

    arc[i] = lv_arc_create(wrapper);
    lv_obj_set_size(arc[i], arc_size, arc_size);
    lv_arc_set_rotation(arc[i], 270);
    lv_arc_set_bg_angles(arc[i], 0, 360);
    lv_arc_set_value(arc[i], 0);
    lv_obj_remove_style(arc[i], NULL, LV_PART_KNOB);
    lv_obj_remove_flag(arc[i], LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *xarclabel = lv_label_create(arc[i]);
    lv_label_set_text(xarclabel, "");
    lv_obj_set_style_text_align(xarclabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(xarclabel, LV_FONT_GET(14), 0);
    lv_obj_center(xarclabel);

    // hitbox overlay as a child of the arc
    lv_obj_t* hit = lv_obj_create(arc[i]);
    lv_obj_remove_style_all(hit);
    lv_obj_set_size(hit, LV_PCT(100), LV_PCT(100));
    lv_obj_center(hit);
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hit, arc_event_cb, LV_EVENT_CLICKED, this);
  }
}

void tab_object_buttonbar::set_click_text(int button, const char *text) {
  if (!text || text[0] == '\0') {
  lv_obj_add_flag(press[buttonmaptoposition[button]], LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(press[buttonmaptoposition[button]], LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lv_obj_get_child(press[buttonmaptoposition[button]], 0), text);
  }
}

void tab_object_buttonbar::set_text(int button, const char *text) {
  lv_label_set_text(lv_obj_get_child(arc[buttonmaptoposition[button]], 0), text);
}

void tab_object_buttonbar::set_ison(int button, bool flag) {
  lv_obj_set_style_arc_color(arc[button], flag?lv_palette_main(LV_PALETTE_LIGHT_BLUE):lv_palette_main(LV_PALETTE_GREY), LV_PART_INDICATOR);
  ison[buttonmaptoposition[button]] = flag;
}

bool tab_object_buttonbar::get_ison(int button) {
  return ison[buttonmaptoposition[button]];
}

char *tab_object_buttonbar::get_text(int button) {
  if (!arc[buttonmaptoposition[button]]) return nullptr;
  if (!onmain[buttonmaptoposition[button]]) return nullptr;
  return lv_label_get_text(lv_obj_get_child(arc[buttonmaptoposition[button]], 0));
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
   
int tab_object_buttonbar::get_value(int button) {
  return lv_arc_get_value(arc[buttonmaptoposition[button]]);
}
   
void tab_object_buttonbar::set_onmain(int button, bool flag) {
  onmain[buttonmaptoposition[button]] = flag;
}

// leds and buttons are numbered differently
void tab_object_buttonbar::set_rgb(int button, lv_color_t rgb) {
  pulsemote_pcb_setleds(button, rgb);
}

void tab_object_buttonbar::set_rgb_all(lv_color_t rgb) {
  for (int button : {tab_object_buttonbar::rotary1, tab_object_buttonbar::rotary2, tab_object_buttonbar::rotary3, tab_object_buttonbar::rotary4, tab_object_buttonbar::switch1}) {
    pulsemote_pcb_setleds(button, rgb);
  }
}