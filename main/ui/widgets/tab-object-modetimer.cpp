#include "tab-object-modetimer.hpp"

tab_object_modetimer::tab_object_modetimer(tab_object_timer *timer, tab_object_timer *rand_timer, StateChangeCb cb)
    : _timer(timer), _rand_timer(rand_timer), _cb(cb) {}

void tab_object_modetimer::start() {
    current_ison = true;
    _cb(true);
    // Initial delay is always from the currently active widget (determined in update)
    // so make update() calculate it immediately
    first_update = true;
}

bool tab_object_modetimer::update(bool is_active_mode, bool is_random_mode) {
    if (!is_active_mode) {
        last_remaining = -1;
        return false;
    }

    bool refresh = false;
    uint32_t now = millis();

    // 1. Check for state toggle. Compare the difference, so it still works when millis() wraps
    if (first_update || (int32_t)(now - next_millis) >= 0) {
        refresh = true;
        if (!first_update) { // Don't toggle on first start call
            current_ison = !current_ison;
            _cb(current_ison);
        }
        first_update = false;

        tab_object_timer *active_widget = is_random_mode ? _rand_timer : _timer;
        int delay_sec = current_ison ? active_widget->gettimeon() : active_widget->gettimeoff();
        next_millis = now + (delay_sec * 1000);
    }

    // 2. Refresh UI if a second passed for the countdown
    int remaining = get_remaining_seconds();
    if (remaining != last_remaining) {
        last_remaining = remaining;
        refresh = true;
    }

    return refresh;
}

int tab_object_modetimer::get_remaining_seconds() const {
    int32_t remaining = (int32_t)(next_millis - (uint32_t)millis());
    if (remaining <= 0) return 0;
    return remaining / 1000;
}
