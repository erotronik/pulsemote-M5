#pragma once

// Home Assistant MQTT discovery: the M5 and each connected device show up in Home Assistant as
// read-only sensors (running, status, mode, battery, the main knob levels and any activity of its
// own, like the bubble bottle's breathing), and button devices as button events

// The M5's availability. Also the broker's last will for us, set when connecting (comms-wifi.cpp)
#define HA_AVAILABILITY_TOPIC "pulsemote/m5/availability"

class Tab;

// Call regularly from the main loop, with the tabs locked
void ha_loop(void);

// A button event ("press", "release" or "hold") from a tab whose has_button_events() is true. From
// the main loop, with the tabs locked
void ha_event(Tab *t, const char *event);
