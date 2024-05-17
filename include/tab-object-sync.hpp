#pragma once

#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>

class tab_object_sync;

class tab_object_sync {
 public:
  tab_object_sync() {};

  // This may get more complex later as you might want to dynamically see what is providing
  // sync data and allow the user to switch between them
  
  lv_obj_t *view(lv_obj_t *parent) {
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
    lv_obj_set_align(container, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_width(container, 160);
    lv_obj_set_height(container, LV_SIZE_CONTENT);
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_style_flex_flow(container, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(container, 10, 0);

    lv_obj_t *label = lv_label_create(container);
    lv_label_set_text(label, "Invert");

    sw = lv_switch_create(container);
    return container;
  };

  void show(bool show) {
    if (container && show) lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
    if (container && !show) lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
  };

  bool isinverted() {
    return lv_obj_has_state(sw, LV_STATE_CHECKED);
  }

 private:
  int value[5];
  static void event_handler(lv_event_t* e);
  bool moderandom;
  lv_obj_t* sw;
  lv_obj_t* container;
};