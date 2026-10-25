#pragma once

#include "sdkconfig.h"
#include "lvgl-utils.h"
#include <unordered_map>

class tab_object_buttonbar {
  public:

    void set_text(int button, const char *text);
    void set_text_fmt(int button, const char *format, ...);
    void set_click_text(int button, const char *text);
    void set_value(int button, int value);
    void set_rgb(int button, lv_color_t rgb);
    void set_rgb_all(lv_color_t rgb);
    void set_onmain(int button, bool flag);
    void set_ison(int button, bool flag);
    // A knob that's a level: sets the arc and a "name\nvalue%" label, and keeps the name and value for
    // Tab::get_levels(). set_text()/set_value() afterwards only change what's shown
    void set_level(int button, const char *name, int value);
    void clear_level(int button);  // no longer a level

    char * get_text(int button);
    int get_value(int button);
    bool get_ison(int button);
    const char *get_level_name(int button);  // nullptr if the knob isn't a level
    int get_level(int button);

    // The board's knobs and switch, as a device's tab has them. The ids are the hardware's numbering
#if CONFIG_PULSEMOTE_PCB || CONFIG_BOARD_M5TAB5
    // The PulseMote PCB: four knobs and a switch. The Tab5 without it has the same, on screen only
    static const int rotary2 = 0;
    static const int rotary1 = 1;
    static const int rotary3 = 2;
    static const int rotary4 = 3;
    static const int switch1 = 4;
    static const int maxrotaries = 4;
    static const int maxbuttons = 5;
#if CONFIG_BOARD_M5TAB5 && !CONFIG_PULSEMOTE_PCB
    // The splashscreen shows knobs from several devices, and the Tab5's screen has room for more. It
    // also has room for taller badges above the knobs, which are easier to touch
    static const int splash_rotaries = 8;
    static const int badge_pad = 8;
#else
    static const int splash_rotaries = maxrotaries;
    static const int badge_pad = 2;  // above and below a badge's text
#endif
#else
#error "No knob layout for this board"
#endif
    static const int splash_buttons = splash_rotaries + 1;

    // rotaries is how many knobs: maxrotaries, or splash_rotaries for the splashscreen. Those past
    // maxrotaries have the ids from maxbuttons up, and are on screen only
    tab_object_buttonbar(lv_obj_t *parent, int rotaries = maxrotaries);

    int nrotaries, nbuttons;
    int all_order[splash_buttons];      // the ids in their order on screen, the switch in the middle
    int rotary_order[splash_rotaries];  // the knobs' ids, in order

    lv_obj_t *container;
    static void arc_event_cb(lv_event_t * e);

  private:
    int buttonmaptoposition[splash_buttons];  // each id's place in all_order
    lv_obj_t *arc[splash_buttons];
    lv_obj_t *press[splash_buttons];
    bool onmain[splash_buttons] = {};
    bool ison[splash_buttons] = {};
    char level_name[splash_buttons][16] = {};
    int level_value[splash_buttons] = {};
};
