#include "tab-object-timer.hpp"

#include <NimBLEDevice.h>
#include <esp_log.h>

#include <functional>
#include <map>

tab_object_timer::tab_object_timer(bool irandom) { moderandom = irandom; }

bool tab_object_timer::has_active_button(void) { return (active_btn != NULL); }

void tab_object_timer::rotary_change(int change) {
  if (!container) return;
  if (!active_btn) return;
  uint32_t *id_ptr = (uint32_t *)lv_obj_get_user_data(active_btn);
  int32_t id = *id_ptr - 1;
  value[id] += change;
  value[id] = max(1, value[id]);
  if (moderandom) {
    // special cases so ranges make sense
    if (id == 0) value[1] = max(value[0], value[1]);
    if (id == 1) value[0] = min(value[0], value[1]);
    if (id == 2) value[3] = max(value[2], value[3]);
    if (id == 3) value[2] = min(value[2], value[3]);
  }
  lv_obj_t *btns = container;
  int j = 4;
  if (!moderandom) j = 2;
  for (int i = 0; i < j; i++) {
    lv_obj_t *btnn = lv_obj_get_child(btns, i);
    if (btnn) lv_label_set_text_fmt(lv_obj_get_child(btnn, 0), "%d", value[i]);
  }
}

void tab_object_timer::highlight_next_field() {
  if (!container) return;
  if (!active_btn) {
    active_btn = lv_obj_get_child(container, 0);
    lv_obj_add_state(active_btn, LV_STATE_CHECKED);
  } else {
    uint32_t *id_ptr = (uint32_t *)lv_obj_get_user_data(active_btn);
    int32_t id = *id_ptr;
    lv_obj_clear_state(active_btn, LV_STATE_CHECKED);
    if ((moderandom && id > 3) || (!moderandom && id > 1)) {
      active_btn = NULL;
    } else {
      active_btn =
          lv_obj_get_child(container, id);  // null is okay for last one
      if (active_btn) lv_obj_add_state(active_btn, LV_STATE_CHECKED);
    }
  }
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
  if (container && show) lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
  if (container && !show) lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
}

int tab_object_timer::gettimeon(void) {
  if (moderandom)
    return random(value[0], value[1]);
  else
    return value[0];
}

int tab_object_timer::gettimeoff(void) {
  if (moderandom)
    return random(value[2], value[3]);
  else
    return value[1];
}

lv_obj_t *tab_object_timer::view(lv_obj_t *tv2) {
  lv_obj_t *timerc = lv_obj_create(tv2);
  lv_obj_set_align(timerc, LV_ALIGN_TOP_RIGHT);
  lv_obj_set_style_pad_top(timerc, 3, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(timerc, 3, LV_PART_MAIN);
  lv_obj_set_style_pad_left(timerc, 3, LV_PART_MAIN);
  lv_obj_set_style_pad_right(timerc, 3, LV_PART_MAIN);

  lv_obj_set_size(timerc, 160, 76);

  active_btn = NULL;
  container = timerc;

  value[0] = 5;
  value[1] = 20;
  value[2] = 20;
  value[3] = 30;
  if (!moderandom) {
    value[0] = 20;
    value[1] = 40;
  }
  static uint32_t id1 = 1, id2 = 2, id3 = 3, id4 = 4;

  static lv_style_t style_checked;
  lv_style_init(&style_checked);
  lv_style_set_bg_color(&style_checked, lv_palette_main(LV_PALETTE_BLUE));
  lv_style_set_bg_opa(&style_checked, LV_OPA_COVER);

  if (moderandom) {
    lv_obj_t *t1a = lv_button_create(timerc);
    lv_obj_align(t1a, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_size(t1a, 55, 30);
    lv_obj_set_style_bg_color(t1a, lv_color_hex(0x000044), LV_PART_MAIN);
    lv_obj_t *t1a1 = lv_label_create(t1a);
    lv_label_set_text_fmt(t1a1, "%d", value[0]);
    lv_obj_set_style_text_align(t1a1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(t1a, event_handler, LV_EVENT_ALL, this);
    lv_obj_add_flag(t1a, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_user_data(t1a, &id1);
    lv_obj_add_style(t1a, &style_checked, LV_STATE_CHECKED);

    lv_obj_t *t2a = lv_button_create(timerc);
    lv_obj_align(t2a, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_size(t2a, 55, 30);
    lv_obj_set_style_bg_color(t2a, lv_color_hex(0x000044), LV_PART_MAIN);
    lv_obj_t *t2a1 = lv_label_create(t2a);
    lv_label_set_text_fmt(t2a1, "%d", value[1]);
    lv_obj_set_style_text_align(t2a1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(t2a, event_handler, LV_EVENT_ALL, this);
    lv_obj_add_flag(t2a, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_user_data(t2a, &id2);
    lv_obj_add_style(t2a, &style_checked, LV_STATE_CHECKED);

    lv_obj_t *t3a = lv_button_create(timerc);
    lv_obj_align(t3a, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_size(t3a, 55, 30);
    lv_obj_set_style_bg_color(t3a, lv_color_hex(0x000044), LV_PART_MAIN);
    lv_obj_t *t3a1 = lv_label_create(t3a);
    lv_label_set_text_fmt(t3a1, "%d", value[2]);
    lv_obj_set_style_text_align(t3a1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(t3a, event_handler, LV_EVENT_ALL, this);
    lv_obj_add_flag(t3a, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_user_data(t3a, &id3);
    lv_obj_add_style(t3a, &style_checked, LV_STATE_CHECKED);

    lv_obj_t *t4a = lv_button_create(timerc);
    lv_obj_align(t4a, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_size(t4a, 55, 30);
    lv_obj_set_style_bg_color(t4a, lv_color_hex(0x000044), LV_PART_MAIN);
    lv_obj_t *t4a1 = lv_label_create(t4a);
    lv_label_set_text_fmt(t4a1, "%d", value[3]);
    lv_obj_set_style_text_align(t4a1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(t4a, event_handler, LV_EVENT_ALL, this);
    lv_obj_add_flag(t4a, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_user_data(t4a, &id4);
    lv_obj_add_style(t4a, &style_checked, LV_STATE_CHECKED);

    lv_obj_t *t1 = lv_label_create(timerc);
    lv_obj_align(t1, LV_ALIGN_TOP_MID, 0, 6);
    lv_label_set_text(t1, "On");
    lv_obj_set_size(t1, 70, 30);
    lv_obj_set_style_text_align(t1, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *t2 = lv_label_create(timerc);
    lv_obj_align(t2, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_label_set_text(t2, "Off");
    lv_obj_set_style_text_align(t2, LV_TEXT_ALIGN_CENTER, 0);
  } else {
    // lv_obj_set_size(timerc, 160, 76);

    lv_obj_t *t2a = lv_button_create(timerc);
    lv_obj_align(t2a, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_size(t2a, 55, 30);
    lv_obj_set_style_bg_color(t2a, lv_color_hex(0x000044), LV_PART_MAIN);
    lv_obj_t *t2a1 = lv_label_create(t2a);
    lv_label_set_text_fmt(t2a1, "%d", value[0]);
    lv_obj_set_style_text_align(t2a1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(t2a, event_handler, LV_EVENT_ALL, this);
    lv_obj_add_flag(t2a, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_user_data(t2a, &id1);
    lv_obj_add_style(t2a, &style_checked, LV_STATE_CHECKED);

    lv_obj_t *t4a = lv_button_create(timerc);
    lv_obj_align(t4a, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_size(t4a, 55, 30);
    lv_obj_set_style_bg_color(t4a, lv_color_hex(0x000044), LV_PART_MAIN);
    lv_obj_t *t4a1 = lv_label_create(t4a);
    lv_label_set_text_fmt(t4a1, "%d", value[1]);
    lv_obj_set_style_text_align(t4a1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(t4a, event_handler, LV_EVENT_ALL, this);
    lv_obj_add_flag(t4a, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_user_data(t4a, &id2);
    lv_obj_add_style(t4a, &style_checked, LV_STATE_CHECKED);

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

  return timerc;
}