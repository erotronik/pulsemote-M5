#include <NimBLEDevice.h>
#include <esp_log.h>

#include "device-funosr.hpp"
#include "comms-bt.hpp"
#include "device.hpp"
#include <functional>
#include <map>

// Connect to a BLE UART sketch on a device connected to the FUNOSR serial port (internally)
// we need to do that because the FUNOSR is only Bluetooth Classic by default
// the device advertises UART and manufacturer 0xf3f3

NimBLEUUID funosr_SERVICE_BLEUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
NimBLEUUID funosr_UUID_RX("6e400003-b5a3-f393-e0a9-e50e24dcca9e");
NimBLEUUID funosr_UUID_TX("6e400002-b5a3-f393-e0a9-e50e24dcca9e");

bool device_funosr::is_device(const NimBLEAdvertisedDevice* advertisedDevice) {
  if (advertisedDevice->isAdvertisingService(funosr_SERVICE_BLEUUID)) {
    uint8_t *md = (uint8_t *)advertisedDevice->getManufacturerData().data();
    if (md && md[0] == 0xf3 && md[1] == 0xf3) 
      return true;
  }
  return false;
}

void device_funosr::set_callback(device_callback c) { update_callback = c; }

void device_funosr::notify(type_of_change change) {
  if (update_callback) update_callback(change, this);
}

void device_funosr::connected_callback() {
  ESP_LOGI(getShortName(), "Client onConnect");
}

void device_funosr::disconnected_callback(int reason) {
  is_connected = false;
  ESP_LOGI(getShortName(), "Client onDisconnect reason: %d", reason);
  notify(D_DISCONNECTED);
}

class DevicefunosrNimBLEClientCallback : public NimBLEClientCallbacks {
 public:
  DevicefunosrNimBLEClientCallback(device_funosr* instance) {
    device_funosr_instance = instance;
  }

  void onConnect(NimBLEClient* pclient) {
    device_funosr_instance->connected_callback();
  }

  // arduino
  void onDisconnect(NimBLEClient* pclient) {
    device_funosr_instance->disconnected_callback(0);
  }

  // esp-idf
  void onDisconnect(NimBLEClient* pclient, int reason) {
    device_funosr_instance->disconnected_callback(reason);
  }

 private:
  device_funosr* device_funosr_instance;
};

device_funosr::device_funosr() {
}

device_funosr::~device_funosr() {
  if (bleClient) {
    NimBLEDevice::deleteClient(bleClient);
    bleClient = nullptr;
  }
}


void device_funosr::ble_funosr_send(const char *newValue) {
  if (is_connected) {
    ESP_LOGI("funosr","Sending %s" ,newValue);
    device_funosr::uuid_tx_Characteristic->writeValue(newValue, strlen(newValue));
  } else 
    ESP_LOGE("funosr","cant send not connected");
}

void device_funosr::funosr_stroke(int smin, int smax, int duration) {
  char msg[20];
  smin = std::max(0, std::min(smin, 99));
  smax = std::max(0, std::min(smax, 99));
  duration = std::max(0, std::min(duration, 999999));
  snprintf(msg, sizeof(msg), "S0%02d%02dI%d\n",smin, smax, duration);
  ble_funosr_send(msg);
}

void device_funosr::funosr_stop(void) {
  ble_funosr_send("DSTOP\n"); 
}

void device_funosr::funosr_goto(int pos, int duration) {
  char msg[20];
  pos = std::min(99,pos);
  if (duration !=0)
    sprintf(msg,"L0%02dI%d\n", pos, duration);
  else
    sprintf(msg,"L0%02d\n", pos);
  ble_funosr_send(msg); 
}

// don't need to parse anything really
void device_funosr::ble_mk_callback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData,
    size_t length, bool isNotify) {
}

bool device_funosr::connect_to_device(NimBLEAdvertisedDevice* device) {
  ESP_LOGI(getShortName(), "Connecting");

  if (!bleClient) {
    bleClient = NimBLEDevice::createClient();
    bleClient->setClientCallbacks(new DevicefunosrNimBLEClientCallback(this));
  }
  notify(D_CONNECTING);
  bool res = true;

  ESP_LOGI(getShortName(), "Will try to connect to %s",device->getAddress().toString().c_str());

  if (!bleClient->connect(device)) {
    ESP_LOGE(getShortName(), "Connection failed");
    return false;
  }
  ESP_LOGI(getShortName(), "Connection established");
  res = ble_get_service(funosrService, bleClient, funosr_SERVICE_BLEUUID);
  if (!res) {
    ESP_LOGE(getShortName(), "Missing service");
    bleClient->disconnect();
    return false;
  }

  res = ble_get_characteristic(funosrService, uuid_rx_Characteristic, funosr_UUID_RX,
      std::bind(&device_funosr::ble_mk_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

  if (!res) {
    ESP_LOGE(getShortName(), "Missing rx characteristic");
    bleClient->disconnect();
    return false;
  }
  res = ble_get_characteristic(funosrService, uuid_tx_Characteristic, funosr_UUID_TX, nullptr);

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