#include <NimBLEDevice.h>
#include <esp_log.h>

#include "comms-bt.hpp"
#include <device-zc95.hpp>
#include <device.hpp>
#include <functional>

// The ZC95 in "BLE serial access" mode (Config -> Remote access) advertises as "ZC95" with the
// Nordic UART service, and carries the same JSON messages as its serial and WiFi remote access
// (see docs/JSON.md in the zc95 repository). Each message is framed by STX and ETX; an EOT makes
// the box stop any running pattern and reset the connection, as does disconnecting.

static NimBLEUUID ZC95_SERVICE_BLEUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
static NimBLEUUID ZC95_UUID_RX("6e400002-b5a3-f393-e0a9-e50e24dcca9e");  // we write
static NimBLEUUID ZC95_UUID_TX("6e400003-b5a3-f393-e0a9-e50e24dcca9e");  // box notifies

static const char STX = 0x02;
static const char ETX = 0x03;
static const char EOT = 0x04;

static const TickType_t REPLY_TIMEOUT = pdMS_TO_TICKS(3000);

bool device_zc95::is_device(const NimBLEAdvertisedDevice* advertisedDevice) {
  // Other devices use the Nordic UART service too, so check the name as well
  return advertisedDevice->isAdvertisingService(ZC95_SERVICE_BLEUUID) && advertisedDevice->getName() == "ZC95";
}

void device_zc95::set_callback(device_callback c) {
  update_callback = c;
}

void device_zc95::notify(type_of_change change) {
  if (update_callback) update_callback(change, this);
}

void device_zc95::connected_callback() {
  ESP_LOGI(getShortName(), "Client onConnect");
}

void device_zc95::disconnected_callback(int reason) {
  is_connected = false;
  running_pattern = -1;
  ESP_LOGI(getShortName(), "Client onDisconnect reason: %d", reason);
  notify(D_DISCONNECTED);
}

class DeviceZC95NimBLEClientCallback : public NimBLEClientCallbacks {
 public:
  DeviceZC95NimBLEClientCallback(device_zc95* instance) {
    device_zc95_instance = instance;
  }

  void onConnect(NimBLEClient* pclient) {
    device_zc95_instance->connected_callback();
  }

  void onDisconnect(NimBLEClient* pclient, int reason) {
    device_zc95_instance->disconnected_callback(reason);
  }

 private:
  device_zc95* device_zc95_instance;
};

device_zc95::device_zc95() {
  request_mutex = xSemaphoreCreateMutex();
  message_queue = xQueueCreate(MESSAGE_QUEUE_LENGTH, sizeof(std::string*));
}

device_zc95::~device_zc95() {
  if (bleClient) {
    if (bleClient->isConnected() && rxCharacteristic) {
      std::string eot(1, EOT);
      send(eot);
    }
    NimBLEDevice::deleteClient(bleClient);
    bleClient = nullptr;
  }
  if (request_mutex) vSemaphoreDelete(request_mutex);
  if (message_queue) {
    std::string* msg = nullptr;
    while (xQueueReceive(message_queue, &msg, 0) == pdTRUE) delete msg;
    vQueueDelete(message_queue);
  }
}

// Write a message to the box, split to fit the connection's MTU. Each part is acknowledged by the
// box, so they can't overrun it.
void device_zc95::send(const std::string& msg) {
  if (!rxCharacteristic) return;

  size_t chunk = bleClient->getMTU() - 3;
  for (size_t pos = 0; pos < msg.length(); pos += chunk) {
    size_t len = std::min(chunk, msg.length() - pos);
    if (!rxCharacteristic->writeValue((const uint8_t*)msg.data() + pos, len, true)) {
      ESP_LOGW(getShortName(), "write of %u bytes failed", (unsigned)len);
      return;
    }
  }
}

// Send a message and wait for the reply with the same MsgId. Returns true if the reply's Result is OK.
bool device_zc95::request(JsonDocument& msg, JsonDocument* reply) {
  if (!rxCharacteristic) return false;

  xSemaphoreTake(request_mutex, portMAX_DELAY);

  int msg_id = ++last_msg_id;
  msg["MsgId"] = msg_id;
  std::string json;
  serializeJson(msg, json);  // replaces the string's contents, so the framing is added after
  std::string out = STX + json + ETX;

  // Deal with anything that arrived since the last request, so the reply doesn't queue behind it
  JsonDocument doc;
  while (next_message(doc, 0)) {}

  ESP_LOGD(getShortName(), "> %s", json.c_str());
  send(out);

  // Wait for the reply, handling any other messages that arrive first
  bool got_reply = false;
  TickType_t start = xTaskGetTickCount();
  while (!got_reply && (xTaskGetTickCount() - start) < REPLY_TIMEOUT) {
    if (next_message(doc, REPLY_TIMEOUT - (xTaskGetTickCount() - start)) && (doc["MsgId"] | 0) == msg_id)
      got_reply = true;
  }

  bool ok = false;
  if (got_reply) {
    ok = (strcmp(doc["Result"] | "", "OK") == 0);
    if (!ok) ESP_LOGW(getShortName(), "%s failed: %s", msg["Type"] | "?", doc["Error"] | "no error given");
    if (reply) *reply = doc;
  } else {
    ESP_LOGW(getShortName(), "No reply to %s", msg["Type"] | "?");
  }

  xSemaphoreGive(request_mutex);
  return ok;
}

// Take the next complete message from the queue, waiting up to wait ticks, and parse it into doc.
// Messages the box sends by itself (MsgId -1) are dealt with here too. Returns false if none came.
bool device_zc95::next_message(JsonDocument& doc, TickType_t wait) {
  std::string* msg = nullptr;
  if (xQueueReceive(message_queue, &msg, wait) != pdTRUE) return false;

  ESP_LOGD(getShortName(), "< %s", msg->c_str());
  DeserializationError err = deserializeJson(doc, *msg);
  delete msg;
  if (err) {
    ESP_LOGW(getShortName(), "JSON parse error: %s", err.c_str());
    doc.clear();
    return true;
  }

  if (doc["Type"] == "LuaScriptOutput") {
    ESP_LOGI(getShortName(), "Script %s: %s", doc["TextType"] | "", doc["Text"] | "");
  }

  // Sent when the front panel knobs change, among other things
  if (doc["Type"] == "PowerStatus") {
    for (JsonObject c : doc["Channels"].as<JsonArray>()) {
      int channel = c["Channel"] | 0;
      if (channel >= 1 && channel <= 4) power_limit[channel - 1] = c["PowerLimit"] | 1000;
    }
  }
  return true;
}

// Deal with messages the box has sent by itself (e.g. PowerStatus), so they don't fill the queue
void device_zc95::poll(void) {
  if (xSemaphoreTake(request_mutex, 0) != pdTRUE) return;  // a request is already reading them
  JsonDocument doc;
  while (next_message(doc, 0)) {}
  xSemaphoreGive(request_mutex);
}

// Called by NimBLE for each notification. Messages can be split over several notifications.
void device_zc95::ble_zc95_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (!isNotify || !pData) return;

  for (size_t i = 0; i < length; i++) {
    char c = pData[i];
    if (c == STX) {
      rx_buffer.clear();
      rx_in_message = true;
    } else if (c == ETX && rx_in_message) {
      // Parsing happens in the task that's waiting for it: this runs in NimBLE's task, which has
      // little stack
      rx_in_message = false;
      std::string* msg = new std::string(std::move(rx_buffer));
      rx_buffer.clear();
      if (xQueueSend(message_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(getShortName(), "Message queue full, message dropped");
        delete msg;
      }
    } else if (rx_in_message) {
      if (rx_buffer.length() < MAX_MESSAGE_SIZE) {
        rx_buffer += c;
      } else {
        ESP_LOGW(getShortName(), "Message too long, discarded");
        rx_in_message = false;
      }
    }
  }
}

bool device_zc95::get_version(void) {
  JsonDocument msg, reply;
  msg["Type"] = "GetVersion";
  if (!request(msg, &reply)) return false;

  ESP_LOGI(getShortName(), "ZC95 firmware %s", reply["ZC95"] | "?");
  return true;
}

bool device_zc95::get_patterns(void) {
  JsonDocument msg, reply;
  msg["Type"] = "GetPatterns";
  if (!request(msg, &reply)) return false;

  patterns.clear();
  for (JsonObject p : reply["Patterns"].as<JsonArray>()) {
    patterns.push_back({p["Id"] | 0, p["Name"] | "?"});
  }

  // The pattern list widget wants an array of C strings
  pattern_names.clear();
  for (const pattern_t& p : patterns) pattern_names.push_back(p.name.c_str());

  ESP_LOGI(getShortName(), "%d patterns", (int)patterns.size());
  return !patterns.empty();
}

const char* device_zc95::get_pattern_name(int index) {
  if (index < 0 || index >= (int)patterns.size()) return "";
  return patterns[index].name.c_str();
}

bool device_zc95::pattern_start(int index) {
  if (index < 0 || index >= (int)patterns.size()) return false;

  // A pattern can only be started when none is running
  if (running_pattern != -1) pattern_stop();

  JsonDocument msg;
  msg["Type"] = "PatternStart";
  msg["Index"] = patterns[index].id;
  if (!request(msg)) return false;

  running_pattern = index;
  return true;
}

bool device_zc95::pattern_stop(void) {
  if (running_pattern == -1) return true;

  JsonDocument msg;
  msg["Type"] = "PatternStop";
  running_pattern = -1;
  return request(msg);
}

bool device_zc95::set_power(int chan1, int chan2, int chan3, int chan4) {
  // Only accepted while a pattern is running
  if (running_pattern == -1) return false;

  JsonDocument msg;
  msg["Type"] = "SetPower";
  msg["Chan1"] = chan1;
  msg["Chan2"] = chan2;
  msg["Chan3"] = chan3;
  msg["Chan4"] = chan4;
  return request(msg);
}

bool device_zc95::connected(void) {
  return is_connected && bleClient && bleClient->isConnected();
}

void device_zc95::disconnect_and_delete(void) {
  bleClient->disconnect();
  vTaskDelay(pdMS_TO_TICKS(100));
  NimBLEDevice::deleteClient(bleClient);
  bleClient = nullptr;
  rxCharacteristic = nullptr;
  notify(D_DISCONNECTED);
}

bool device_zc95::connect_to_device(NimBLEAdvertisedDevice* device) {
  ESP_LOGI(getShortName(), "Connecting core%d", xPortGetCoreID());

  if (!bleClient) {
    bleClient = NimBLEDevice::createClient();
    bleClient->setClientCallbacks(new DeviceZC95NimBLEClientCallback(this));
  }
  notify(D_CONNECTING);

  ESP_LOGI(getShortName(), "Will try to connect to %s", device->getAddress().toString().c_str());
  if (!bleClient->connect(device)) {
    ESP_LOGE(getShortName(), "Connection failed");
    return false;
  }
  ESP_LOGI(getShortName(), "Connection established, MTU %d", bleClient->getMTU());

  NimBLERemoteCharacteristic* txCharacteristic = nullptr;
  bool res = ble_get_service(zc95Service, bleClient, ZC95_SERVICE_BLEUUID) &&
             ble_get_characteristic(zc95Service, rxCharacteristic, ZC95_UUID_RX) &&
             // Subscribe with a write request: the ZC95's BLE stack ignores write-without-response to
             // the notification descriptor, which would leave it never sending us anything
             ble_get_characteristic(zc95Service, txCharacteristic, ZC95_UUID_TX,
                 std::bind(&device_zc95::ble_zc95_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
                 true);
  if (!res) {
    ESP_LOGE(getShortName(), "Missing service or characteristic");
    bleClient->disconnect();
    rxCharacteristic = nullptr;
    return false;
  }
  is_connected = true;

  // Check it's really talking to a ZC95, and get the patterns it can run
  vTaskDelay(pdMS_TO_TICKS(100));
  if (!get_version() || !get_patterns()) {
    ESP_LOGE(getShortName(), "Couldn't get version and patterns from box");
    is_connected = false;
    disconnect_and_delete();
    return false;
  }

  notify(D_CONNECTED);
  return true;
}
