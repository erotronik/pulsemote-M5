#include <functional>

#include "lvgl-utils.h"
#include "tab-object-timer.hpp"

static void sanitize_lv_objects(lv_obj_t *&container, lv_obj_t *&active_btn, bool &is_visible) {
  if (container && !lv_obj_is_valid(container)) {
    container = nullptr;
    active_btn = nullptr;
    is_visible = false;
    return;
  }
  if (active_btn && !lv_obj_is_valid(active_btn)) {
    active_btn = nullptr;
  }
}

tab_object_timer::tab_object_timer(bool irandom, bool ion_only) {
  moderandom = irandom;
  mode_on_only = ion_only;
  active_btn = nullptr;
  container = nullptr;
  is_visible = false;
}

bool tab_object_timer::has_focus(void) {
  sanitize_lv_objects(container, active_btn, is_visible);
  return (active_btn  && container && is_visible);
}

void tab_object_timer::rotary_change(int change) {
  sanitize_lv_objects(container, active_btn, is_visible);
  if (!container) return;
  if (!active_btn) return;
  if (!is_visible) return;

  uint32_t *id_ptr = (uint32_t *)lv_obj_get_user_data(active_btn);
  int32_t id = *id_ptr - 1;
  if (mode_on_only && id != 0) return;
  value[id] = std::max<int>(1, value[id]+change);
  if (moderandom) {
    // special cases so ranges make sense
    if (id == 0) value[1] = std::max(value[0], value[1]);
    if (id == 1) value[0] = std::min(value[0], value[1]);
    if (id == 2) value[3] = std::max(value[2], value[3]);
    if (id == 3) value[2] = std::min(value[2], value[3]);
  }
  // Clamp ON time(s) to max_on
  if (max_on > 0) {
    value[0] = std::min(value[0], max_on);
    if (moderandom) value[1] = std::min(value[1], max_on);
  }
  uint32_t child_count = lv_obj_get_child_count(container);
  for (uint32_t i = 0; i < child_count; i++) {
    lv_obj_t *btnn = lv_obj_get_child(container, i);
    if (!btnn) continue;
    if (!lv_obj_has_flag(btnn, LV_OBJ_FLAG_CHECKABLE)) continue;

    uint32_t *btn_id_ptr = (uint32_t *)lv_obj_get_user_data(btnn);
    if (!btn_id_ptr || *btn_id_ptr == 0) continue;
    int32_t btn_id = *btn_id_ptr - 1;
    if (btn_id < 0 || btn_id >= 5) continue;

    lv_obj_t *label = lv_obj_get_child(btnn, 0);
    if (label) lv_label_set_text_fmt(label, "%d", value[btn_id]);
  }
}

bool tab_object_timer::highlight_next_field() {
  sanitize_lv_objects(container, active_btn, is_visible);
  if (!container) return false;
  if (!is_visible) return false;
  if (!active_btn) {
    active_btn = lv_obj_get_child(container, 0);
    lv_obj_add_state(active_btn, LV_STATE_CHECKED);
  } else {
    uint32_t *id_ptr = (uint32_t *)lv_obj_get_user_data(active_btn);
    int32_t id = *id_ptr;
    lv_obj_clear_state(active_btn, LV_STATE_CHECKED);
    if ((moderandom && id > 3) || (mode_on_only && id > 0) || (!moderandom && id > 1)) {
      active_btn = NULL;
    } else {
      active_btn = lv_obj_get_child(container, id);  // null is okay for last one
      if (active_btn) lv_obj_add_state(active_btn, LV_STATE_CHECKED);
    }
  }
  return active_btn;
}

void tab_object_timer::event_handler(lv_event_t *e) {
  tab_object_timer *instance =
      static_cast<tab_object_timer *>(lv_event_get_user_data(e));

  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);

  if (code == LV_EVENT_CLICKED) {
    if (obj ==
        instance->active_btn) {  // If the current button is already active
      lv_obj_clear_state(obj, LV_STATE_CHECKED);  // Unpress the button
      instance->active_btn = NULL;                // No active button
    } else {
      if (instance->active_btn != NULL) {
        lv_obj_clear_state(
            instance->active_btn,
            LV_STATE_CHECKED);  // Unpress the previously active button
      }
      lv_obj_add_state(obj, LV_STATE_CHECKED);  // Press the new button
      instance->active_btn = obj;  // Update the active button pointer
    }
  }
}

void tab_object_timer::show(bool show) {
  sanitize_lv_objects(container, active_btn, is_visible);
  if (container && show) {
    lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
    is_visible = true;
  } else if (container && !show) {
    lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
    is_visible = false;
  }
}

int tab_object_timer::gettimeon(void) {
  if (moderandom)
    return value[0] + rand() % (value[1] - value[0]);
  else
    return value[0];
}

int tab_object_timer::gettimeoff(void) {
  if (mode_on_only) return 0;
  if (moderandom)
    return value[2] + rand() % (value[3] - value[2]);
  else
    return value[1];
}

static lv_obj_t *make_grid_btn(lv_obj_t *parent, lv_align_t align, int value, void *user_data,  void *t, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x000044), LV_PART_MAIN);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_ALL, t);
    lv_obj_set_size(btn, LV_SCALE(55), LV_SCALE(30));
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_user_data(btn, user_data);
    lv_obj_add_style(btn, &lvpulsemote_style_checked, LV_STATE_CHECKED);
    lv_obj_align(btn, align, 0, 0);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text_fmt(label, "%d", value);
    lv_obj_set_style_text_font(label, LV_FONT_GET(14), 0);
    lv_obj_center(label);
    return btn;
}

void tab_object_timer::view(lv_obj_t *tv2) {
  lv_obj_t *timerc = lv_obj_create(tv2);
  lv_obj_align(timerc, LV_ALIGN_TOP_RIGHT, 0, LV_SCALE(dropdown_height+6));
  lv_obj_set_style_pad_all(timerc, LV_SCALE(3), LV_PART_MAIN);
  lv_obj_set_size(timerc, LV_SCALE(dropdown_width), LV_SCALE(mode_on_only ? 42 : 76));

  active_btn = NULL;
  container = timerc;

  if (moderandom) {
    value[0] = 5; value[1] = 10; value[2] = 20; value[3] = 30;
  } else if (mode_on_only) {
    value[0] = 5;
  } else {
    value[0] = 5; value[1] = 10;
  }
  static uint32_t ids[4] = { 1, 2, 3, 4 };

  if (moderandom) {
    (void)make_grid_btn(timerc, LV_ALIGN_TOP_LEFT, value[0], &ids[0], this, event_handler); 
    (void)make_grid_btn(timerc, LV_ALIGN_TOP_RIGHT, value[1], &ids[1], this, event_handler);
    (void)make_grid_btn(timerc, LV_ALIGN_BOTTOM_LEFT, value[2], &ids[2], this, event_handler);
    (void)make_grid_btn(timerc, LV_ALIGN_BOTTOM_RIGHT, value[3], &ids[3], this, event_handler);

    lv_obj_t *t1 = lv_label_create(timerc);
    lv_obj_align(t1, LV_ALIGN_TOP_MID, 0, LV_SCALE(6));
    lv_label_set_text(t1, "On");
    lv_obj_set_size(t1, LV_SCALE(70), LV_SCALE(30));
    lv_obj_set_style_text_align(t1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(t1, LV_FONT_GET(14), 0);

    lv_obj_t *t2 = lv_label_create(timerc);
    lv_obj_align(t2, LV_ALIGN_BOTTOM_MID, 0, LV_SCALE(-6));
    lv_label_set_text(t2, "Off");
    lv_obj_set_style_text_align(t2, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(t2, LV_FONT_GET(14), 0);
  } else if (mode_on_only) {
    lv_obj_t *btn = make_grid_btn(timerc, LV_ALIGN_RIGHT_MID, value[0], &ids[0], this, event_handler);
    lv_obj_align(btn, LV_ALIGN_RIGHT_MID, 0, LV_SCALE(1));

    lv_obj_t *t1 = lv_label_create(timerc);
    lv_obj_align(t1, LV_ALIGN_LEFT_MID, LV_SCALE(10), LV_SCALE(1));
    lv_label_set_text(t1, "On");
    lv_obj_set_size(t1, LV_SCALE(70), LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(t1, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(t1, LV_FONT_GET(14), 0);
  } else {
    // lv_obj_set_size(timerc, dropdown_width, 76);

    (void)make_grid_btn(timerc, LV_ALIGN_TOP_RIGHT, value[0], &ids[0], this, event_handler);
    (void)make_grid_btn(timerc, LV_ALIGN_BOTTOM_RIGHT, value[1], &ids[1], this, event_handler);

    lv_obj_t *t1 = lv_label_create(timerc);
    lv_obj_align(t1, LV_ALIGN_TOP_MID, 0, 6);
    lv_label_set_text(t1, "On");
    lv_obj_set_size(t1, 70, 30);
    lv_obj_set_style_text_align(t1, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *t2 = lv_label_create(timerc);
    lv_obj_align(t2, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_label_set_text(t2, "Off");
    lv_obj_set_style_text_align(t2, LV_TEXT_ALIGN_CENTER, 0);
  }
  show(false);
}