#include "tab-object-sync.hpp"
#include "lvgl-utils.h"

void tab_object_sync::view(lv_obj_t *parent) {
  container = lv_obj_create(parent);
  lv_obj_set_size(container, LV_SCALE(dropdown_width), LV_SIZE_CONTENT);
  lv_obj_align(container, LV_ALIGN_TOP_RIGHT, 0, LV_SCALE(dropdown_height+6));

  lv_obj_set_layout(container, LV_LAYOUT_FLEX);
  lv_obj_set_style_flex_flow(container, LV_FLEX_FLOW_ROW, 0);
  lv_obj_set_flex_align(container, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_set_style_pad_all(container, LV_SCALE(3), LV_PART_MAIN);
  lv_obj_set_style_pad_gap(container, LV_SCALE(10), 0);

  lv_obj_t *label = lv_label_create(container);
  lv_label_set_text(label, "Invert");
  lv_obj_set_style_text_font(label, LV_FONT_GET(14), 0);

  sw = lv_switch_create(container);
  show(false);
}

void tab_object_sync::show(bool show) {
  if (container && show) lv_obj_remove_flag(container, LV_OBJ_FLAG_HIDDEN);
  if (container && !show) lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
}

bool tab_object_sync::isinverted() {
  return lv_obj_has_state(sw, LV_STATE_CHECKED);
}
