#include <esp_log.h>
#include <cstring>
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

std::string Tab::get_status_text(void) {
    if (!status) return "";
    const char *text = status->get_text();
    return std::string(text, strcspn(text, "\n"));
}

int Tab::get_levels(level_t levels[levels_max]) {
    if (!buttonbar) return 0;
    int n = 0;
    for (int i = 0; i < tab_object_buttonbar::maxbuttons && n < levels_max; i++) {
        int b = buttonbar->rotary_order[i];
        if (b == -1) continue;
        // The label is like "1\n45%": the name is the first line. Skip LVGL symbols, which are in the
        // private use area so start with 0xEF in UTF-8
        const char *label = buttonbar->get_text(b);
        if (!label || !*label || (unsigned char)label[0] == 0xEF) continue;
        size_t len = strcspn(label, "\n");
        if (len >= sizeof(levels[n].name)) len = sizeof(levels[n].name) - 1;
        memcpy(levels[n].name, label, len);
        levels[n].name[len] = '\0';
        levels[n].value = buttonbar->get_value(b);
        n++;
    }
    return n;
}

void Tab::create_battery_label(lv_obj_t *parent) {
    battery_label = lv_obj_create(parent);
    lv_obj_set_size(battery_label, LV_SCALE(150), LV_SCALE(24));
    lv_obj_align(battery_label, LV_ALIGN_TOP_LEFT, LV_SCALE(4), LV_SCALE(64+12));
    lv_obj_t *labelx = lv_label_create(battery_label);
    lv_label_set_text(labelx, "");
    lv_obj_set_style_text_font(labelx, LV_FONT_GET(14), LV_PART_MAIN);
    lv_obj_set_style_text_align(labelx, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_ver(battery_label, LV_SCALE(3), LV_PART_MAIN);
    lv_obj_align(labelx, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_scrollbar_mode(battery_label, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
    shown_battery = -1;
}

void Tab::set_battery_label(int percent) {
    if (!battery_label || percent == shown_battery) return;
    shown_battery = percent;
    if (percent < 0) {
        lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text_fmt(lv_obj_get_child(battery_label, 0), "battery %d%%", percent);
    lv_obj_clear_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
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
