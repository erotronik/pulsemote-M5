//#define LV_CONF_INCLUDE_SIMPLE
#include "lvgl-utils.h"
#include "tab-splashscreen.hpp"
#include <tab.hpp>

lv_style_t lvpulsemote_style_status;
lv_style_t lvpulsemote_style_tab;
lv_style_t lvpulsemote_style_checked;

float _lv_scale = 1.0f;

const lv_font_t* lv_font_get_scaled(uint32_t size) {
    uint32_t scaled_size = (uint32_t)(size * _lv_scale);
    
#ifdef CONFIG_BOARD_M5TAB5
    if (scaled_size >= 48) return &lv_font_montserrat_42;
    if (scaled_size >= 40) return &lv_font_montserrat_34;
    if (scaled_size >= 32) return &lv_font_montserrat_28;
    return &lv_font_montserrat_28; // Absolute minimum for high-res
#else
    if (scaled_size >= 24) return &lv_font_montserrat_24;
    if (scaled_size >= 16) return &lv_font_montserrat_16;
    return &lv_font_montserrat_14;
#endif
}

void lv_init_pulsemote(void) {
  // Determine scale based on screen width
  // We डिजाइन for 320px on small screens, but on high-res screens (like Tab5 1280px)
  // we scale by 2x instead of 4x to give more real estate and avoid "huge" UI.
  lv_display_t * disp = lv_display_get_default();
  if (disp) {
      uint32_t width = lv_display_get_horizontal_resolution(disp);
      if (width > 480) {
          _lv_scale = (float)width / 640.0f; // 1280 -> 2.0
      } else {
          _lv_scale = 1.0f; // 320 -> 1.0
      }
  }
  ESP_LOGI("lvgl", "UI Scale factor: %.2f", _lv_scale);

  lv_style_init(&lvpulsemote_style_status);
  lv_style_set_bg_color(&lvpulsemote_style_status, lv_color_hex(0xFF0000));
  lv_style_set_pad_ver(&lvpulsemote_style_status, LV_SCALE(3));
  lv_style_set_text_font(&lvpulsemote_style_status, LV_FONT_GET(24));
  lv_style_set_text_align(&lvpulsemote_style_status, LV_TEXT_ALIGN_CENTER);

  lv_style_init(&lvpulsemote_style_tab);
  lv_style_set_pad_all(&lvpulsemote_style_tab, 0);
  lv_style_set_pad_top(&lvpulsemote_style_tab, LV_SCALE(10));
  lv_style_set_bg_opa(&lvpulsemote_style_tab, LV_OPA_COVER);
  lv_style_set_bg_color(&lvpulsemote_style_tab, lv_color_hex(0x000000));

  lv_style_init(&lvpulsemote_style_checked);
  lv_style_set_bg_color(&lvpulsemote_style_checked, lv_palette_main(LV_PALETTE_BLUE));
  lv_style_set_bg_opa(&lvpulsemote_style_checked, LV_OPA_COVER);

}

// Log to serial, and if the splashscreen is there, also to the debug window of the splashscreen

void printf_log(const char *format, ...) {
  char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  ESP_LOGD("log","%s",buf);
  
  // Try to update the splashscreen log, but skip if we can't get the lock immediately.
  // This prevents crashes if called from timer callbacks or during contention.
  TabLock lock(tabs_mutex, 0); 
  if (lock && !tabs.empty()) {
    Tab *t = tabs.front();
    tab_splashscreen *ts = static_cast<tab_splashscreen *>(t);
    if (ts && ts->lv_debug_window) {
      TabLock l_lock(lvgl_mutex, 0);
      if (l_lock) {
        lv_textarea_add_text(ts->lv_debug_window, buf);
      }
    }
  }
}

// given a tabview and a content page of a tab, get the tab index. This
// stops us having to keep track of the tabid which changes when we
// delete tabs as hardware goes away

int lv_get_tabview_idx_from_page(lv_obj_t *l, lv_obj_t *page) {
  lv_obj_t *c = lv_tabview_get_content(l);
  for (int i = 0; i < lv_obj_get_child_count(c); i++) {
    if (lv_obj_get_child(c, i) == page) return i;
  }
  return -1;
}

void lv_hide_tab(lv_obj_t *page) {
  if (!page) return;
  int tabid = lv_get_tabview_idx_from_page(tv, page);
  ESP_LOGD("main", "hide tab id %d", tabid);
  if (tabid == -1) return;
  // if we're viewing the tab that's gone away then switch to the main screen
  if (lv_tabview_get_tab_act(tv) == tabid)
    lv_tabview_set_act(tv, 0, LV_ANIM_OFF);

  lv_obj_t *tbar = lv_tabview_get_tab_bar(tv);  // in lvgl 9 they are real buttons not a matrix
  lv_obj_t *cont = lv_tabview_get_content(tv);
  lv_obj_del(lv_obj_get_child(tbar, tabid));
  lv_obj_del(lv_obj_get_child(cont, tabid));
}
