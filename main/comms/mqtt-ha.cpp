#include <esp_log.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <map>
#include <string>

#include "mqtt-ha.hpp"
#include "comms-wifi.hpp"
#include "tab.hpp"
#include "board.hpp"
#include "lvgl-utils.h"

// Each device publishes its state as JSON on pulsemote/<id>/state (not retained), and the discovery messages tell
// Home Assistant which entity reads which part of it. Devices also have pulsemote/<id>/availability,
// not retained but sent again every minute, so a device that was connected before the M5 restarted
// doesn't stay "available" in the broker forever

static const unsigned long state_interval_ms = 1000;      // at most this often per device
static const unsigned long heartbeat_interval_ms = 60000; // state and availability resent anyway
static const int discovery_messages_max = 10;             // the most one device announces at once
static const int expire_after_s = 150;                    // a device's sensors go unavailable without a heartbeat

struct ha_device {
  std::string name;        // as on its tab
  std::string last_state;  // last JSON sent
  unsigned long last_sent = 0;
  unsigned long online_due = 0;  // availability, sent every heartbeat however often the state changes
  bool announced = false;
  bool battery_announced = false;
  bool events_announced = false;
  uint32_t knobs_announced = 0;  // bit per level, as from Tab::get_levels()
  bool seen = false;             // still has a tab, this pass
};

static std::map<std::string, ha_device> devices;
static bool was_connected = false;
static unsigned long last_pass = 0;
static unsigned long m5_last_sent = 0;
static std::string m5_last_state;

// Topic-safe id from a tab name: lower case, no dashes (like the pulsemote/<name> sync topics)
static std::string device_id(const char *name) {
  std::string id;
  for (const char *c = name; *c; c++) {
    if (*c == '-') continue;
    id += isalnum((unsigned char)*c) ? (char)tolower((unsigned char)*c) : '_';
  }
  return id;
}

// Button presses, as an event entity (see ha_event())
static const char *button_events[] = {"press", "release", "hold"};

// One entity's discovery message. dev_id "m5" is the M5 itself, which the devices sit behind.
// An "event" entity reads pulsemote/<dev_id>/event, and has no value template
// optional_key: the entity is unavailable while the state doesn't have this (like the MK312's levels,
// which it only has while the M5 sets them)
static void announce(const char *component, const std::string &dev_id, const std::string &dev_name,
                     const std::string &object, const std::string &name, const char *value_template,
                     const char *unit = nullptr, const char *device_class = nullptr,
                     const char *optional_key = nullptr) {
  const bool m5 = dev_id == "m5";
  const bool event = !strcmp(component, "event");
  JsonDocument doc;
  doc["name"] = name;
  doc["uniq_id"] = "pulsemote_" + dev_id + "_" + object;
  doc["obj_id"] = "pulsemote_" + dev_id + "_" + object;
  doc["stat_t"] = "pulsemote/" + dev_id + (event ? "/event" : "/state");
  if (value_template) doc["val_tpl"] = value_template;
  if (event) {
    JsonArray types = doc["event_types"].to<JsonArray>();
    for (const char *type : button_events) types.add(type);
  }
  if (unit) doc["unit_of_meas"] = unit;
  if (device_class) doc["dev_cla"] = device_class;
  if (!strcmp(component, "sensor") && unit) doc["stat_cla"] = "measurement";
  // In case the M5 went away without saying the device had gone (like a restart), which it can't
  // after the event as it doesn't remember what was connected
  if (!m5 && !event) doc["exp_aft"] = expire_after_s;

  JsonArray avty = doc["avty"].to<JsonArray>();
  avty.add<JsonObject>()["t"] = HA_AVAILABILITY_TOPIC;
  if (!m5) {
    avty.add<JsonObject>()["t"] = "pulsemote/" + dev_id + "/availability";
    doc["avty_mode"] = "all";
  }
  if (optional_key) {
    JsonObject has_key = avty.add<JsonObject>();
    has_key["t"] = "pulsemote/" + dev_id + "/state";
    has_key["val_tpl"] = std::string("{{ 'online' if value_json.") + optional_key + " is defined else 'offline' }}";
  }

  JsonObject dev = doc["dev"].to<JsonObject>();
  dev["ids"].to<JsonArray>().add("pulsemote_" + dev_id);
  dev["name"] = m5 ? std::string("PulseMote") : "PulseMote " + dev_name;
  dev["mf"] = "PulseMote";
  if (!m5) dev["via_device"] = "pulsemote_m5";

  char message[max_message_size];
  if (measureJson(doc) >= sizeof(message)) {
    ESP_LOGE("ha", "discovery message for %s %s too long", dev_id.c_str(), object.c_str());
    return;
  }
  serializeJson(doc, message, sizeof(message));
  std::string topic = std::string("homeassistant/") + component + "/pulsemote_" + dev_id + "/" + object + "/config";
  mqttsend(topic.c_str(), message, true);
}

// Only the M5's own state is retained: a device's would outlive it if the M5 restarted
static void send_state(const std::string &id, const std::string &state) {
  mqttsend(("pulsemote/" + id + "/state").c_str(), state.c_str(), id == "m5");
}

static void m5_pass(unsigned long now) {
  if (!was_connected) {
    mqttsend(HA_AVAILABILITY_TOPIC, "online", true);
    announce("sensor", "m5", "", "battery", "Battery", "{{ value_json.battery }}", "%", "battery");
    announce("binary_sensor", "m5", "", "charging", "Charging", "{{ value_json.charging }}", nullptr, "battery_charging");
    m5_last_state.clear();
  }

  JsonDocument doc;
  doc["battery"] = hardware_get_battery_level();
  doc["charging"] = hardware_is_charging() ? "ON" : "OFF";
  std::string state;
  serializeJson(doc, state);
  if (state != m5_last_state || now - m5_last_sent > heartbeat_interval_ms) {
    send_state("m5", state);
    m5_last_state = state;
    m5_last_sent = now;
  }
}

static const char *mode_name(Tab::main_modes mode) {
  switch (mode) {
    case Tab::MODE_MANUAL: return "Manual";
    case Tab::MODE_TIMER:  return "Timer";
    case Tab::MODE_RANDOM: return "Random";
    case Tab::MODE_SYNC:   return "Sync";
  }
  return "";
}

static void device_pass(Tab *t, unsigned long now) {
  const char *name = t->gettabname();
  std::string id = device_id(name);
  if (id.empty() || id == "m5") return;
  ha_device &d = devices[id];
  d.seen = true;

  if (!d.announced) {
    if (mqttsend_space() < discovery_messages_max) return;  // next time, when the queue has room
    d.name = name;
    // Without a status (like the DG button, which has no page) there's no running, status or mode
    if (t->status) {
      announce("binary_sensor", id, name, "running", "Running", "{{ value_json.on }}", nullptr, "running");
      announce("sensor", id, name, "status", "Status", "{{ value_json.status }}");
      announce("sensor", id, name, "mode", "Mode", "{{ value_json.mode }}");
    }
    if (t->has_button_events()) {
      announce("event", id, name, "button", "Button", nullptr, nullptr, "button");
      d.events_announced = true;
    }
    d.announced = true;
    d.last_sent = 0;
    // After Home Assistant has had a moment to subscribe to it, from the discovery messages
    d.online_due = now + 3000;
  }

  JsonDocument doc;
  if (t->status) {
    doc["on"] = t->status->get_active() ? "ON" : "OFF";
    doc["status"] = t->get_status_text();
    doc["mode"] = mode_name(t->main_mode);
  }

  int battery = t->get_battery();
  if (battery >= 0) {
    doc["battery"] = battery;
    if (!d.battery_announced && mqttsend_space() > 2) {
      announce("sensor", id, d.name, "battery", "Battery", "{{ value_json.battery }}", "%", "battery");
      d.battery_announced = true;
    }
  }

  Tab::level_t levels[Tab::levels_max];
  int level_count = t->get_levels(levels);
  for (int i = 0; i < level_count; i++) {
    std::string key = "level_" + device_id(levels[i].name);
    doc[key] = levels[i].value;
    if (!(d.knobs_announced & (1u << i)) && mqttsend_space() > 2) {
      std::string tpl = "{{ value_json." + key + " }}";
      announce("sensor", id, d.name, key, std::string("Level ") + levels[i].name, tpl.c_str(), "%", nullptr, key.c_str());
      d.knobs_announced |= 1u << i;
    }
  }

  std::string state;
  if (doc.isNull()) doc.to<JsonObject>();  // {} rather than null, for a device with nothing to report yet
  serializeJson(doc, state);
  bool heartbeat = now - d.last_sent > heartbeat_interval_ms;
  if ((state != d.last_state && now - d.last_sent > state_interval_ms) || heartbeat) {
    if (mqttsend_space() < 2) return;
    send_state(id, state);
    d.last_state = state;
    d.last_sent = now;
  }
  if ((long)(now - d.online_due) >= 0 && mqttsend_space() >= 1) {
    mqttsend(("pulsemote/" + id + "/availability").c_str(), "online");
    d.online_due = now + heartbeat_interval_ms;
  }
}

void ha_event(Tab *t, const char *event) {
  if (!was_connected || !is_wifi_connected()) return;
  auto it = devices.find(device_id(t->gettabname()));
  if (it == devices.end() || !it->second.events_announced) return;  // not announced yet
  JsonDocument doc;
  doc["event_type"] = event;
  std::string message;
  serializeJson(doc, message);
  mqttsend(("pulsemote/" + it->first + "/event").c_str(), message.c_str());
}

void ha_loop(void) {
  unsigned long now = millis();
  if (now - last_pass < 250) return;
  last_pass = now;

  if (!is_wifi_connected()) {
    was_connected = false;
    return;
  }
  if (!was_connected) {
    // A new connection to the broker: announce everything again
    devices.clear();
  }
  m5_pass(now);
  was_connected = true;

  for (auto &[id, d] : devices) d.seen = false;
  for (Tab *t : tabs)
    if (t->device) device_pass(t, now);

  // Devices whose tab has gone
  for (auto it = devices.begin(); it != devices.end();) {
    if (!it->second.seen) {
      mqttsend(("pulsemote/" + it->first + "/availability").c_str(), "offline");
      it = devices.erase(it);
    } else {
      ++it;
    }
  }
}
