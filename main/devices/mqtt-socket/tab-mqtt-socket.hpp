#pragma once

#include "lvgl-utils.h"
#include "tab.hpp"

class tab_mqtt_socket : public Tab {
 public:
  tab_mqtt_socket(char *n, char *t);
  ~tab_mqtt_socket();
  void encoder_change(int sw, int change) override;
  void switch_change(int sw, bool state) override;
  void loop(bool activetab) override;
  void focus_change(bool focus) override;
  bool hardware_changed(void) override;
  void gotsyncdata(Tab *t, sync_data status) override;
  void mqtt_received(const char *topic, const char *message) override;

  void setup() override {
    tab_create();
  }

  const char* gettabname(void) override { return mqtt_topic_name;};

  const char *mqtt_socket_main_modes_c =  "Manual\nTimer\nRandom\nSync";

 private:
  char mqtt_topic[100] = "";        // where we send ON and OFF, zigbee2mqtt/<name>/set
  char mqtt_state_topic[100] = "";  // where Zigbee2MQTT says what it is, zigbee2mqtt/<name>
  char mqtt_topic_name[20] = "";
  void tab_create(void);
  bool need_knob_refresh = false;
  uint8_t preset = 0;
  bool ison;

  // Zigbee2MQTT says what the socket is once the socket's confirmed it (with "optimistic" off for
  // it), so after sending ON or OFF we wait for that, and show if it doesn't come
  int reported = -1;  // what Zigbee2MQTT last said: 1 on, 0 off, -1 not yet
  bool wanted = false;
  unsigned long waiting_since = 0;
  bool no_reply = false;
  void send_state(bool on);
  bool lockpanel = false;
  int level_a, level_b;
};
