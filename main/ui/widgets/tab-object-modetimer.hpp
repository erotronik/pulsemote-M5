#pragma once

#include "lvgl-utils.h"
#include "tab-object-timer.hpp"
#include <functional>

/**
 * @brief Helper class to manage the Random and Timer mode state machine.
 * Reduces 40+ lines of duplicated code in every tab loop().
 */
class tab_object_modetimer {
public:
    typedef std::function<void(bool is_on)> StateChangeCb;

    /**
     * @param timer Pointer to the manual timer widget
     * @param rand_timer Pointer to the random timer widget
     * @param cb Callback triggered when state toggles (On -> Off or Off -> On)
     */
    tab_object_modetimer(tab_object_timer *timer, tab_object_timer *rand_timer, StateChangeCb cb);

    /**
     * @brief Run this in the tab's loop().
     * @param is_active_mode True if main_mode is either TIMER or RANDOM
     * @param is_random_mode True if main_mode is specifically RANDOM
     * @return true if UI refresh is needed (state changed or a second passed)
     */
    bool update(bool is_active_mode, bool is_random_mode);

    /**
     * @brief Call when transitioning into Timer/Random mode to start the first cycle.
     */
    void start();

    /**
     * @brief Synchronize the internal state if manual override happens (e.g. Stop button).
     */
    void set_is_on(bool is_on) { current_ison = is_on; }
    
    bool is_on() const { return current_ison; }
    int get_remaining_seconds() const;

private:
    tab_object_timer *_timer;
    tab_object_timer *_rand_timer;
    StateChangeCb _cb;

    bool current_ison = false;
    uint32_t next_millis = 0;
    int last_remaining = -1;
};
