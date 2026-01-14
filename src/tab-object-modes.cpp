#include "tab-object-modes.hpp"

tab_object_modes::tab_object_modes() {};

void tab_object_modes::createdropdown(lv_obj_t *parent, const char *options) {
  dd = lv_dropdown_create(parent);
  lv_obj_set_style_text_font(dd, LV_FONT_GET(16), LV_PART_MAIN);
  lv_obj_set_style_text_font(lv_dropdown_get_list(dd), LV_FONT_GET(16), LV_PART_MAIN);
  lv_obj_set_align(dd, LV_ALIGN_TOP_RIGHT);
  lv_obj_set_size(dd, LV_SCALE(dropdown_width), LV_SCALE(dropdown_height));  // match the timer box width
  lv_dropdown_set_options(dd, options);
}

bool tab_object_modes::has_focus() {
  return (lv_dropdown_is_open(dd));
}

bool tab_object_modes::highlight_next_field() {
  if (!lv_dropdown_is_open(dd)) {
    lv_dropdown_open(dd);
    return true;
  }
  lv_dropdown_close(dd);
  lv_obj_send_event(dd, LV_EVENT_VALUE_CHANGED, NULL);
  return false;
}

void tab_object_modes::reset() {
  lv_dropdown_set_selected(dd, 0);
  lv_obj_send_event(dd, LV_EVENT_VALUE_CHANGED, NULL);
}

bool tab_object_modes::rotary_change(int change) {
  if (lv_dropdown_is_open(dd)) {
    uint16_t selected_id = lv_dropdown_get_selected(dd);
    uint16_t option_count = lv_dropdown_get_option_count(dd);
    uint16_t next_id = (selected_id + change) % option_count;
    lv_dropdown_set_selected(dd, next_id); 
    return true;
  }
  return false;
}