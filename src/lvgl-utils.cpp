#define LV_CONF_INCLUDE_SIMPLE
#include <esp_timer.h>
#include <lvgl.h>
#include "lvgl-utils.h"
#include "tab-splashscreen.hpp"
#include <tab.hpp>

lv_style_t lvpulsemote_style_status;
lv_style_t lvpulsemote_style_tab;
lv_style_t lvpulsemote_style_checked;

void lv_init_pulsemote(void) {
  lv_style_init(&lvpulsemote_style_status);
  lv_style_set_bg_color(&lvpulsemote_style_status, lv_color_hex(0xFF0000));
  lv_style_set_pad_ver(&lvpulsemote_style_status, 3);
  lv_style_set_text_font(&lvpulsemote_style_status, &lv_font_montserrat_24);
  lv_style_set_text_align(&lvpulsemote_style_status, LV_TEXT_ALIGN_CENTER);

  lv_style_init(&lvpulsemote_style_tab);
  lv_style_set_pad_all(&lvpulsemote_style_tab, 0);
  lv_style_set_pad_top(&lvpulsemote_style_tab, 10);
  lv_style_set_bg_opa(&lvpulsemote_style_tab, LV_OPA_COVER);
  lv_style_set_bg_color(&lvpulsemote_style_tab, lv_color_hex(0x000000));

  lv_style_init(&lvpulsemote_style_checked);
  lv_style_set_bg_color(&lvpulsemote_style_checked, lv_palette_main(LV_PALETTE_BLUE));
  lv_style_set_bg_opa(&lvpulsemote_style_checked, LV_OPA_COVER);

}

// Log to serial, and if the splashscreen is there, also to the debug window of the splashscreen

void printf_log(const char *format, ...) {
  static char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, 255, format, args);
  va_end(args);
  Serial.print(buf);
  if (tabs.size() > 0) {
    Tab *t = tabs.front();
    tab_splashscreen *ts = static_cast<tab_splashscreen *>(t);
    if (ts)
      lv_textarea_add_text(ts->lv_debug_window, buf);
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

  lv_obj_t *tbar = lv_tabview_get_tab_bar(
      tv);  // in lvgl 9 they are real buttons not a matrix
  lv_obj_t *cont = lv_tabview_get_content(tv);
  lv_obj_del(lv_obj_get_child(tbar, tabid));
  lv_obj_del(lv_obj_get_child(cont, tabid));
}