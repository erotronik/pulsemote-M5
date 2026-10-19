#include "tab.hpp"
#include "lvgl-utils.h" // for printf_log()
#include "device-dgbutton.hpp" 
#include "tab-dgbutton.hpp"
#include "tab-splashscreen.hpp"
#include "board.hpp" // for hardware_beep()

tab_dgbutton::tab_dgbutton() {
  page = nullptr;
  old_last_change = last_change = D_NONE;
  device = nullptr;
}

tab_dgbutton::~tab_dgbutton() {
  if (battery_icon) lv_obj_delete(battery_icon);
}

void tab_dgbutton::loop(bool activetab) {
  device_dgbutton *md = static_cast<device_dgbutton *>(device);
  device_dgbutton::dgbutton_event state;
  update_battery_icon();
  if (xQueueReceive(md->events,&state, 0)) {
    if (state == device_dgbutton::dgbutton_event::PUSH) {
      held = false;
      send_sync_data(SYNC_BUTTONPRESS);
    }
    if (state == device_dgbutton::dgbutton_event::RELEASE) {
      held = false;
      send_sync_data(SYNC_BUTTONRELEASE);
    }
    // The button repeats HELD every second: only act on the first
    if (state == device_dgbutton::dgbutton_event::HELD && !held) {
      held = true;
      printf_log("DG Button Pushed\n");
      send_sync_data(SYNC_ALLOFF);
      hardware_beep();
    }
  }
}

// A hexagon like the button, filled from the bottom up to the battery level
static void battery_icon_draw_cb(lv_event_t *e) {
  lv_obj_t *obj = lv_event_get_target_obj(e);
  device_dgbutton *md = static_cast<device_dgbutton *>(lv_event_get_user_data(e));
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  int32_t w = lv_area_get_width(&a) - 1, h = lv_area_get_height(&a) - 1;
  int32_t x0 = a.x1, y0 = a.y1;

  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = lv_palette_main(LV_PALETTE_BLUE);
  d.width = 1;

  // Flat top and bottom: full width across the middle, half width at the top and bottom
  int level = std::max(0, std::min(100, md->get_battery()));
  for (int32_t y = h - (h * level) / 100; y <= h; y++) {
    int32_t hw = w / 2 - (w * abs(2 * y - h)) / (4 * h);
    d.p1.x = x0 + w / 2 - hw; d.p1.y = y0 + y;
    d.p2.x = x0 + w / 2 + hw; d.p2.y = y0 + y;
    lv_draw_line(layer, &d);
  }

  d.width = LV_SCALE(2);
  const int32_t px[7] = {w / 4, 3 * w / 4, w, 3 * w / 4, w / 4, 0, w / 4};
  const int32_t py[7] = {0, 0, h / 2, h, h, h / 2, 0};
  for (int i = 0; i < 6; i++) {
    d.p1.x = x0 + px[i];     d.p1.y = y0 + py[i];
    d.p2.x = x0 + px[i + 1]; d.p2.y = y0 + py[i + 1];
    lv_draw_line(layer, &d);
  }
}

// We have no page of our own, so show the button's battery on the splashscreen's status icons
void tab_dgbutton::update_battery_icon(void) {
  device_dgbutton *md = static_cast<device_dgbutton *>(device);
  if (md->get_battery() == battery_shown) return;
  battery_shown = md->get_battery();
  if (!battery_icon) {
    for (const auto& t : tabs) {
      if (t->type != DeviceType::splashscreen) continue;
      lv_obj_t *icons = static_cast<tab_splashscreen *>(t)->icons_bg;
      if (!icons) return;
      battery_icon = lv_obj_create(icons);
      lv_obj_remove_style_all(battery_icon);
      lv_obj_set_size(battery_icon, LV_SCALE(20), LV_SCALE(18));
      lv_obj_move_to_index(battery_icon, 0);
      lv_obj_add_event_cb(battery_icon, battery_icon_draw_cb, LV_EVENT_DRAW_MAIN, md);
    }
  }
  if (battery_icon) lv_obj_invalidate(battery_icon);
}
  
// return false if we removed ourselves from the connected devices list
bool tab_dgbutton::hardware_changed(void) {
  if (last_change == D_CONNECTING) {
    printf_log("Connecting %s\n", device->getShortName());
  } else if (last_change == D_CONNECTED) {
    device_dgbutton *md = static_cast<device_dgbutton *>(device);
    xQueueReset(md->events);
    printf_log("Connected %s\n", device->getShortName());
  } else if (last_change == D_DISCONNECTED) {
    printf_log("Disconnected %s\n", device->getShortName());
    return false;
  }
  return true;
}
