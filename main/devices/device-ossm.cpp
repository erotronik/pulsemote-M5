#include <NimBLEDevice.h>
#include <esp_log.h>
#include <ArduinoJson.h>

#include "device-ossm.hpp"
#include "comms-bt.hpp"
#include "device.hpp"
#include <functional>
#include <map>

// OSSM BLE implementation as of Oct 2025

NimBLEUUID ossm_SERVICE_BLEUUID("522b443a-4f53-534d-0001-420badbabe69");
NimBLEUUID ossm_TX("522b443a-4f53-534d-1000-420badbabe69");
NimBLEUUID ossm_RX("522b443a-4f53-534d-2000-420badbabe69");
NimBLEUUID ossm_SPEEDKNOB("522b443a-4f53-534d-1010-420badbabe69");
NimBLEUUID ossm_PATTERNLIST("522b443a-4f53-534d-3000-420badbabe69");

bool device_ossm::is_device(const NimBLEAdvertisedDevice* advertisedDevice) {
  return (advertisedDevice->isAdvertisingService(ossm_SERVICE_BLEUUID));
}

void device_ossm::set_callback(device_callback c) { 
  update_callback = c; 
}

void device_ossm::notify(type_of_change change) {
  if (update_callback) update_callback(change, this);
}

void device_ossm::ble_ossm_send(const char* newValue) {
  if (!is_connected) 
    return;
  ESP_LOGI("ossm","Sending %s" ,newValue);
  device_ossm::ossm_tx_Characteristic->writeValue(newValue, strlen(newValue));
}

void device_ossm::set_speed(int speed) {
  std::snprintf(send_buf, sizeof(send_buf), "set:speed:%d", speed);
  ble_ossm_send(send_buf);
}

void device_ossm::set_stroke(int s) {
  std::snprintf(send_buf, sizeof(send_buf), "set:stroke:%d", s);
  ble_ossm_send(send_buf);
}

void device_ossm::set_depth(int s) {
  std::snprintf(send_buf, sizeof(send_buf), "set:depth:%d", s);
  ble_ossm_send(send_buf);
}

void device_ossm::set_sensation(int s) {
  std::snprintf(send_buf, sizeof(send_buf), "set:sensation:%d", s);
  ble_ossm_send(send_buf);
}

void device_ossm::set_pattern(int s) {
  std::snprintf(send_buf, sizeof(send_buf), "set:pattern:%d", s);
  ble_ossm_send(send_buf);
}

void device_ossm::connected_callback() {
  ESP_LOGI(getShortName(), "Client onConnect");
}

void device_ossm::disconnected_callback(int reason) {
  ESP_LOGI(getShortName(), "Client onDisconnect reason: %d", reason);
  is_connected = false;
  notify(D_DISCONNECTED);
}

class DeviceossmNimBLEClientCallback : public NimBLEClientCallbacks {
 public:
  DeviceossmNimBLEClientCallback(device_ossm* instance) {
    device_ossm_instance = instance;
  }

  void onConnect(NimBLEClient* pclient) {
    device_ossm_instance->connected_callback();
  }
  // arduino
  void onDisconnect(NimBLEClient* pclient) {
    device_ossm_instance->disconnected_callback(0);
  }
  // esp-idf
  void onDisconnect(NimBLEClient* pclient, int reason) {
    device_ossm_instance->disconnected_callback(reason);
  }
 private:
  device_ossm* device_ossm_instance;
};

device_ossm::device_ossm() {}

device_ossm::~device_ossm() {
  if (bleClient) {
    NimBLEDevice::deleteClient(bleClient);
    bleClient = nullptr;
  }
}

void device_ossm::ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (!isNotify || !pData || length == 0) return;
  //ESP_LOGD("ossm", "Received (%u bytes)", (unsigned)length);
  rxstatus.clear();
  DeserializationError err = deserializeJson(rxstatus, pData, length);
  if (err) {
    ESP_LOGW("ossm", "JSON parse error: %s", err.c_str());
    return;
  }
  //ESP_LOGI("ossm","speed=%d",rxstatus["speed"]|0);
}

bool device_ossm::got_data_yet() {
  return (!rxstatus["speed"].isNull());
}

int device_ossm::get_speed() {
  return (rxstatus["speed"]|0);
}

int device_ossm::get_depth() {
  return (rxstatus["depth"]|0);
}

int device_ossm::get_stroke() {
  return (rxstatus["stroke"]|0);
}

int device_ossm::get_sensation() {
  return (rxstatus["sensation"]|0);
}

int device_ossm::get_pattern() {
  return (rxstatus["pattern"]|0);
}

const char* device_ossm::pattern_name_for_idx(int idx) {
  if (!patternlist.is<JsonArray>()) return "";

  JsonArray arr = patternlist.as<JsonArray>();
  for (JsonObject obj : arr) {
    if ((int)(obj["idx"] | -1) == idx) {
      return obj["name"] | "";
    }
  }
  return ""; // not found
}

bool device_ossm::connect_to_device(NimBLEAdvertisedDevice* device) {
  ESP_LOGI(getShortName(), "Connecting");

  if (!bleClient) {
    bleClient = NimBLEDevice::createClient();
    bleClient->setClientCallbacks(new DeviceossmNimBLEClientCallback(this));
  }
  notify(D_CONNECTING);
  bool res = true;

  ESP_LOGI(getShortName(), "Will try to connect to %s", device->getAddress().toString().c_str());

  if (!bleClient->connect(device)) {
    ESP_LOGE(getShortName(), "Connection failed");
    return false;
  }
  ESP_LOGI(getShortName(), "Connection established");

  res = ble_get_service(ossmService, bleClient, ossm_SERVICE_BLEUUID);
  if (!res) {
    ESP_LOGE(getShortName(), "Missing service");
    bleClient->disconnect();
    return false;
  }

  res = ble_get_characteristic(ossmService, ossm_rx_Characteristic, ossm_RX,
      std::bind(&device_ossm::ble_mk_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
  if (!res) {
    ESP_LOGE(getShortName(), "Missing rx characteristic");
    bleClient->disconnect();
    return false;
  }

  res = ble_get_characteristic(ossmService, ossm_tx_Characteristic, ossm_TX, nullptr);
  if (!res) {
    ESP_LOGE(getShortName(), "Missing tx characteristic");
    bleClient->disconnect();
    return false;
  }

  res = ble_get_characteristic(ossmService, ossm_speedknob_Characteristic, ossm_SPEEDKNOB, nullptr);
  if (!res) {
    ESP_LOGE(getShortName(), "Missing speedknob characteristic");
    bleClient->disconnect();
    return false;
  }

  res = ble_get_characteristic(ossmService, ossm_patternlist_Characteristic, ossm_PATTERNLIST, nullptr);
  if (!res) {
    ESP_LOGE(getShortName(), "Missing patternlist characteristic");
    bleClient->disconnect();
    return false;
  }

  ESP_LOGI(getShortName(), "Found services and characteristics");

  std::string payload = ossm_patternlist_Characteristic->readValue();
  DeserializationError err = deserializeJson(patternlist, payload);
  if (err) {
    ESP_LOGE(getShortName(), "Missing patternlist json");
  }

  const char* newValue = "false";
  ossm_speedknob_Characteristic->writeValue(newValue, strlen(newValue));
  ble_ossm_send("go:strokeEngine");

  is_connected = true;

  notify(D_CONNECTED);
  return true;
}