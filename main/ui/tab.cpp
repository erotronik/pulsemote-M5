#include <esp_log.h>
#include "tab.hpp"
#include "lvgl-utils.h"

lv_obj_t* Tab::create_standard_page(const char* modes_str) {
    page = lv_tabview_add_tab(tv, gettabname());
    lv_obj_add_style(page, &lvpulsemote_style_tab, LV_PART_MAIN);
    
    if (modes_str && modeselect) {
        modeselect->createdropdown(page, modes_str);
        lv_obj_add_event_cb(modeselect->getdropdownobject(), standard_mode_change_cb, LV_EVENT_VALUE_CHANGED, this);
    }
    return page;
}

void Tab::create_standard_widgets() {
    if (!page) return;
    
    buttonbar = new tab_object_buttonbar(page);
    
    // Default status widget, tabs can override alignment if needed
    status = new tab_object_status(page, 150, 64);
    status->align(LV_ALIGN_TOP_LEFT, LV_SCALE(4), 0);
    
    if (rand_timer) rand_timer->view(page);
    if (timer) timer->view(page);
    if (sync) sync->view(page);
    
    lv_tabview_set_act(tv, lv_get_tabview_idx_from_page(tv, page), LV_ANIM_OFF);
    buttonbar->set_click_text(tab_object_buttonbar::rotary4, LV_SYMBOL_SETTINGS);
}

void Tab::update_standard_mode_visibility() {
    if (rand_timer) rand_timer->show(main_mode == MODE_RANDOM);
    if (timer) timer->show(main_mode == MODE_TIMER);
    if (sync) sync->show(main_mode == MODE_SYNC);
}

void Tab::standard_mode_change_cb(lv_event_t *event) {
    Tab *tab = static_cast<Tab *>(lv_event_get_user_data(event));
    if (!tab) return;

    tab->main_mode = static_cast<main_modes>(lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(event)));
    ESP_LOGI("tab", "Mode changed to %d", tab->main_mode);
    
    tab->need_refresh = true;
    if ((tab->main_mode == MODE_RANDOM || tab->main_mode == MODE_TIMER) && tab->modetimer) {
        tab->modetimer->start();
    }
    tab->update_standard_mode_visibility();
}
