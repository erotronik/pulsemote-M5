#include "tab-object-sync.hpp"
#include "lvgl-utils.h"

lv_obj_t *tab_object_sync::view(lv_obj_t *parent) {
  container = lv_obj_create(parent);
  // Set the container to be transparent and have no effect
  //lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
  //lv_obj_set_style_border_opa(container, LV_OPA_TRANSP, 0);
  //lv_obj_set_style_outline_opa(container, LV_OPA_TRANSP, 0);
  //lv_obj_set_style_shadow_opa(container, LV_OPA_TRANSP, 0);
  //lv_obj_set_style_pad_all(container, 0, 0);
  lv_obj_set_style_pad_top(container, 3, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(container, 3, LV_PART_MAIN);
  lv_obj_set_style_pad_left(container, 3, LV_PART_MAIN);
  lv_obj_set_style_pad_right(container, 3, LV_PART_MAIN);
  lv_obj_align(container, LV_ALIGN_TOP_RIGHT, 0, dropdown_height+6);
  lv_obj_set_width(container, dropdown_width);
  lv_obj_set_height(container, LV_SIZE_CONTENT);
  lv_obj_set_layout(container, LV_LAYOUT_FLEX);
  lv_obj_set_style_flex_flow(container, LV_FLEX_FLOW_ROW, 0);
  lv_obj_set_flex_align(container, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(container, 10, 0);

  lv_obj_t *label = lv_label_create(container);
  lv_label_set_text(label, "Invert");

  sw = lv_switch_create(container);
  return container;
}

void tab_object_sync::show(bool show) {
  if (container && show) lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
  if (container && !show) lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
}

bool tab_object_sync::isinverted() {
  return lv_obj_has_state(sw, LV_STATE_CHECKED);
}
