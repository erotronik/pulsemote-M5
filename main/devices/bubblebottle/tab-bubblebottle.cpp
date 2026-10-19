#include "tab.hpp"
#include "lvgl-utils.h" // for printf_log()
#include "device-bubblebottle.hpp" 
#include "tab-bubblebottle.hpp"
#include "tab-splashscreen.hpp"

tab_bubblebottle::tab_bubblebottle() {
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
}

tab_bubblebottle::~tab_bubblebottle() {
  if (icon) lv_obj_delete(icon);
}

static void add_bubble(lv_obj_t *parent, int x, int y, int size) {
  lv_obj_t *b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_set_pos(b, LV_SCALE(x), LV_SCALE(y));
  lv_obj_set_size(b, LV_SCALE(size), LV_SCALE(size));
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(b, LV_SCALE(2), 0);
  lv_obj_set_style_border_color(b, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_set_style_bg_color(b, lv_palette_main(LV_PALETTE_BLUE), 0);
}

// We have no page of our own, so show some bubbles on the splashscreen's status icons, filled in
// while breathing in
void tab_bubblebottle::update_icon(bool breath_in) {
  if (!icon) {
    lv_obj_t *icons = splashscreen_icons();
    if (!icons) return;
    icon = lv_obj_create(icons);
    lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon, LV_SCALE(20), LV_SCALE(18));
    lv_obj_move_to_index(icon, 0);
    add_bubble(icon, 0, 7, 11);
    add_bubble(icon, 11, 4, 8);
    add_bubble(icon, 14, 0, 5);
  }
  for (uint32_t i = 0; i < lv_obj_get_child_count(icon); i++)
    lv_obj_set_style_bg_opa(lv_obj_get_child(icon, i), breath_in ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}

void tab_bubblebottle::loop(bool activetab) {
  device_bubblebottle *md = static_cast<device_bubblebottle *>(device);
  int state;
  if (!icon) update_icon(false);
  if (xQueueReceive(md->events,&state, 0)) {
    update_icon(state == 1);
    send_sync_data(state==1?SYNC_ON:SYNC_OFF);
  }
}
  
// return false if we removed ourselves from the connected devices list
bool tab_bubblebottle::hardware_changed(void) {
  if (last_change == D_CONNECTING) {
    printf_log("Connecting %s\n", device->getShortName());
  } else if (last_change == D_CONNECTED) {
    device_bubblebottle *md = static_cast<device_bubblebottle *>(device);
    send_sync_data(SYNC_START);
    send_sync_data(SYNC_OFF);
    xQueueReset(md->events);
    printf_log("Connected %s\n", device->getShortName());
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", device->getShortName());
    send_sync_data(SYNC_BYE);
    return false;
  }
  return true;
}
