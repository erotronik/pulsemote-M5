#include "tab-object-patterns.hpp"

tab_object_patterns::tab_object_patterns() {};

#include "esp_log.h"


void tab_object_patterns::selectpattern(lv_obj_t *parent, const char *patterns[],int patterns_n) {
  ESP_LOGD("pattern","select called");
  dd = lv_dropdown_create(parent);
  hide();
  lv_obj_set_style_text_font(dd, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_font(lv_dropdown_get_list(dd), &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_align(dd, LV_ALIGN_TOP_LEFT);
  lv_obj_set_size(dd, LV_PCT(100), dropdown_height);  // match the timer box width

  lv_dropdown_clear_options(dd);  // start clean

  for (uint32_t i = 0; i < patterns_n; i++) {
    lv_dropdown_add_option(dd, patterns[i], LV_DROPDOWN_POS_LAST);
  }
}

uint32_t tab_object_patterns::hide() {
  is_visible = false;
  lv_dropdown_close(dd);
  lv_obj_add_flag(dd, LV_OBJ_FLAG_HIDDEN);
  return lv_dropdown_get_selected(dd);
}

void tab_object_patterns::show(int i) {
  is_visible = true;
  lv_obj_clear_flag(dd, LV_OBJ_FLAG_HIDDEN);
  lv_dropdown_set_selected(dd, i);
  lv_dropdown_open(dd);
}

bool tab_object_patterns::visible() {
  return (is_visible);
}

bool tab_object_patterns::highlight_next_field() {
  if (!lv_dropdown_is_open(dd)) {
    lv_dropdown_open(dd);
    return true;
  }
  lv_dropdown_close(dd);
  lv_obj_send_event(dd, LV_EVENT_VALUE_CHANGED, NULL);
  return false;
}

void tab_object_patterns::reset() {
  lv_dropdown_set_selected(dd, 0);
  lv_obj_send_event(dd, LV_EVENT_VALUE_CHANGED, NULL);
}

bool tab_object_patterns::rotary_change(int change) {
  if (lv_dropdown_is_open(dd)) {
    uint16_t selected_id = lv_dropdown_get_selected(dd);
    uint16_t option_count = lv_dropdown_get_option_count(dd);
    uint16_t next_id = (selected_id + change) % option_count;
    lv_dropdown_set_selected(dd, next_id); 
    return true;
  }
  return false;
}