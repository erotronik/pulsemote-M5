#include <NimBLEDevice.h>
#include <esp_log.h>

#include "comms-bt.hpp"
#include "device-loop.hpp"
#include "device.hpp"
#include <functional>
#include <map>

NimBLEUUID loop_SERVICE_BLEUUID("19b10010-e8f2-537e-4f6c-d104768a1214");
NimBLEUUID loop_UUID_RX("19b10013-e8f2-537e-4f6c-d104768a1214");

bool device_loop::is_device(NimBLEAdvertisedDevice* advertisedDevice) {
   return (advertisedDevice->isAdvertisingService(loop_SERVICE_BLEUUID));
}

void device_loop::set_callback(device_callback c) { 
  update_callback = c; 
}

void device_loop::notify(type_of_change change) {
  if (update_callback) update_callback(change, this);
}

void device_loop::connected_callback() {
  ESP_LOGI(getShortName(), "Client onConnect");
}

void device_loop::disconnected_callback(int reason) {
  is_connected = false;
  ESP_LOGI(getShortName(), "Client onDisconnect reason: %d", reason);
  notify(D_DISCONNECTED);
}

class DeviceloopNimBLEClientCallback : public NimBLEClientCallbacks {
 public:
  DeviceloopNimBLEClientCallback(device_loop* instance) {
    device_loop_instance = instance;
  }

  void onConnect(NimBLEClient* pclient){
    device_loop_instance->connected_callback();
  }

  // arduino
  void onDisconnect(NimBLEClient* pclient) {
    device_loop_instance->disconnected_callback(0);
  }

  // esp-idf
  void onDisconnect(NimBLEClient* pclient, int reason) {
    device_loop_instance->disconnected_callback(reason);
  }

 private:
  device_loop* device_loop_instance;
};

device_loop::device_loop() {
  events = xQueueCreate(10,sizeof(int));
  loopreading = 1024;
  nreadings = 0;
}

device_loop::~device_loop() {
  // bleClient->deleteServices(); // deletes all services, which should delete
  // all characteristics NimBLEDevice::deleteClient(bleClient); // will also
  // disconnect
}

int device_loop::get_reading() {
  return (loopreading);
}

void device_loop::ble_mk_callback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData,
    size_t length, bool isNotify) {
  if (nreadings < 1) {
    loopreading = (1024 - *pData);
    nreadings = 1;
  }
  loopreading = 0.9*loopreading + (1024- *pData)*.1;
  xQueueSend(events, &loopreading, 0);
}

bool device_loop::connect_to_device(NimBLEAdvertisedDevice* device) {
  ESP_LOGI(getShortName(), "Connecting");

  if (!bleClient) {
    bleClient = NimBLEDevice::createClient();
    bleClient->setClientCallbacks(new DeviceloopNimBLEClientCallback(this));
  }
  notify(D_CONNECTING);
  bool res = true;

  ESP_LOGI(getShortName(), "Will try to connect to %s", device->getAddress().toString().c_str());

  if (!bleClient->connect(device)) {
    ESP_LOGE(getShortName(), "Connection failed");
    return false;
  }
  ESP_LOGI(getShortName(), "Connection established");
  res &= ble_get_service(loopService, bleClient, loop_SERVICE_BLEUUID);
  if (res == false) {
    ESP_LOGE(getShortName(), "Missing service");
    bleClient->disconnect();
    return false;
  }

  res &= ble_get_characteristic(loopService, uuid_rx_Characteristic, loop_UUID_RX,
      std::bind(&device_loop::ble_mk_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

  if (res == false) {
    ESP_LOGE(getShortName(), "Missing rx characteristic");
    bleClient->disconnect();
    return false;
  }

  ESP_LOGI(getShortName(), "Found services and characteristics");
  is_connected = true;

  notify(D_CONNECTED);
  return true;
}