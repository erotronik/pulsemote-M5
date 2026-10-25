#include <NimBLEDevice.h>
#include <esp_log.h>

#include "device-breath.hpp"
#include "comms-bt.hpp"
#include "device.hpp"
#include "../../boards/board.hpp"
#include <functional>
#include <map>

// the device advertises UART and manufacturer 0xf1f5

NimBLEUUID breath_SERVICE_BLEUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
NimBLEUUID breath_UUID_RX("6e400003-b5a3-f393-e0a9-e50e24dcca9e");
NimBLEUUID breath_UUID_TX("6e400002-b5a3-f393-e0a9-e50e24dcca9e");

bool device_breath::is_device(const NimBLEAdvertisedDevice* advertisedDevice) {
  if (advertisedDevice->isAdvertisingService(breath_SERVICE_BLEUUID)) {
    std::string md = advertisedDevice->getManufacturerData();
    if (md.size() >= 2 && (uint8_t)md[0] == 0xf1 && (uint8_t)md[1] == 0xf5)
      return true;
  }
  return false;
}

void device_breath::set_callback(device_callback c) { update_callback = c; }

void device_breath::notify(type_of_change change) {
  if (update_callback) update_callback(change, this);
}

void device_breath::connected_callback() {
  ESP_LOGI(getShortName(), "Client onConnect");
}

void device_breath::disconnected_callback(int reason) {
  if (!is_connected) {
    ESP_LOGI(getShortName(), "Client onDisconnect (already disconnected), reason: %d", reason);
    return;
  }
  is_connected = false;
  ESP_LOGI(getShortName(), "Client onDisconnect reason: %d", reason);
  notify(D_DISCONNECTED);
}

class DevicebreathNimBLEClientCallback : public NimBLEClientCallbacks {
 public:
  DevicebreathNimBLEClientCallback(device_breath* instance) {
    device_breath_instance = instance;
  }

  void onConnect(NimBLEClient* pclient) {
    device_breath_instance->connected_callback();
  }

  // arduino
  void onDisconnect(NimBLEClient* pclient) {
    device_breath_instance->disconnected_callback(0);
  }

  // esp-idf
  void onDisconnect(NimBLEClient* pclient, int reason) {
    device_breath_instance->disconnected_callback(reason);
  }

 private:
  device_breath* device_breath_instance;
};

device_breath::device_breath() {
}

device_breath::~device_breath() {
  if (bleClient) {
    ble_delete_client(bleClient);
  }
}

void device_breath::ble_breath_send(const char *newValue) {
  if (!is_connected || !bleClient || !bleClient->isConnected()) {
    ESP_LOGE("breath", "cant send not connected");
    disconnected_callback(-1);
    return;
  }

  if (!uuid_tx_Characteristic) {
    ESP_LOGE("breath", "cant send missing tx characteristic");
    disconnected_callback(-2);
    return;
  }

  ESP_LOGI("breath", "Sending %s", newValue);

  // Use write-with-response so link loss is surfaced as a failed write.
  const bool write_ok = device_breath::uuid_tx_Characteristic->writeValue(newValue, strlen(newValue), true);
  if (!write_ok) {
    ESP_LOGE("breath", "write failed, forcing disconnect state");
    disconnected_callback(-3);
    if (bleClient->isConnected()) {
      bleClient->disconnect();
    }
  }
}

void device_breath::breath_block(int pc) {
  if (pc == 100) hardware_beep(10);
  if (pc == 100) 
    ble_breath_send("3\n");
  else if (pc == 50) 
    ble_breath_send("1\n");
  else
    ble_breath_send("0\n");
}

// don't need to parse anything really
void device_breath::ble_mk_callback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData,
    size_t length, bool isNotify) {
}

bool device_breath::connect_to_device(NimBLEAdvertisedDevice* device) {
  ESP_LOGI(getShortName(), "Connecting");

  if (!bleClient) {
    bleClient = NimBLEDevice::createClient();
    bleClient->setClientCallbacks(new DevicebreathNimBLEClientCallback(this));
  }
  notify(D_CONNECTING);
  bool res = true;

  ESP_LOGI(getShortName(), "Will try to connect to %s",device->getAddress().toString().c_str());

  if (!bleClient->connect(device)) {
    ESP_LOGE(getShortName(), "Connection failed");
    return false;
  }
  ESP_LOGI(getShortName(), "Connection established");
  res = ble_get_service(breathService, bleClient, breath_SERVICE_BLEUUID);
  if (!res) {
    ESP_LOGE(getShortName(), "Missing service");
    bleClient->disconnect();
    return false;
  }

  res = ble_get_characteristic(breathService, uuid_rx_Characteristic, breath_UUID_RX,
      std::bind(&device_breath::ble_mk_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

  if (!res) {
    ESP_LOGE(getShortName(), "Missing rx characteristic");
    bleClient->disconnect();
    return false;
  }
  res = ble_get_characteristic(breathService, uuid_tx_Characteristic, breath_UUID_TX, nullptr);

  if (!res) {
    ESP_LOGE(getShortName(), "Missing tx characteristic");
    bleClient->disconnect();
    return false;
  }

  ESP_LOGI(getShortName(), "Found services and characteristics");
  is_connected = true;

  notify(D_CONNECTED);
  return true;
}