#pragma once

bool is_wifi_connected(void);
void wifi_setup(void);
void mqttsend(const char *topic, const char *message);
void mqttsubscribe(const char *topic);
bool wifi_loop(void);