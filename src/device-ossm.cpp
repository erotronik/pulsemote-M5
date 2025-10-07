#include <NimBLEDevice.h>
#include <esp_log.h>

#include "device-ossm.hpp"
#include "comms-bt.hpp"
#include "device.hpp"
#include <functional>
#include <map>

// OSSM BLE implementation as of Oct 2025

NimBLEUUID ossm_SERVICE_BLEUUID("522b443a-4f53-534d-0001-420badbabe69");
//NimBLEUUID ossm_TX("522b443a-4f53-534d-0002-420badbabe69");
NimBLEUUID ossm_RX("522b443a-4f53-534d-1000-420badbabe69");
NimBLEUUID ossm_SPEEDKNOB("522b443a-4f53-534d-1010-420badbabe69"); // not 0010

bool device_ossm::is_device(NimBLEAdvertisedDevice* advertisedDevice) {
  if (advertisedDevice->isAdvertisingService(ossm_SERVICE_BLEUUID))
    //if (strnstr(advertisedDevice->getName().c_str(),"OSSM",4))
      return true;
  return false;
}

void device_ossm::set_callback(device_callback c) { 
  update_callback = c; 
}

void device_ossm::notify(type_of_change change) {
  if (update_callback) update_callback(change, this);
}


void device_ossm::ble_ossm_send(String newValue) {
  if (!is_connected) 
    return;
  //xQueueReset(notifyQueue);
  ESP_LOGI("ossm","Sending %s" ,newValue);
  device_ossm::ossm_rx_Characteristic->writeValue(newValue.c_str(), newValue.length());
  //NotifyPacket received;

  //if (xQueueReceive(notifyQueue, &received, pdMS_TO_TICKS(200))) {
  //  char buf[100];
  // int len = received.length < 99? received.length: 99;
  //  strncat(buf,(const char *)received.data,len);
  //  buf[len]=0;
  //  ESP_LOGI("lovense","command returned %s",buf);
  //}
}

void device_ossm::set_speed(int speed) {
  ble_ossm_send("set:speed:" +String(speed));
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

device_ossm::~device_ossm() {}

void device_ossm::ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  ESP_LOGD("ossm", "Received (%u bytes): %.*s", (unsigned)length, (int)length, (char*)pData);
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
  res &= ble_get_service(ossmService, bleClient, ossm_SERVICE_BLEUUID);
  if (res == false) {
    ESP_LOGE(getShortName(), "Missing service");
    bleClient->disconnect();
    return false;
  }

  res &= ble_get_characteristic(ossmService, ossm_rx_Characteristic, ossm_RX,
      std::bind(&device_ossm::ble_mk_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

  if (res == false) {
    ESP_LOGE(getShortName(), "Missing rx characteristic");
    bleClient->disconnect();
    return false;
  }

  res &= ble_get_characteristic(ossmService, ossm_speedknob_Characteristic, ossm_SPEEDKNOB, nullptr);

  if (res == false) {
    ESP_LOGE(getShortName(), "Missing speedknob characteristic");
    bleClient->disconnect();
    return false;
  }

  ESP_LOGI(getShortName(), "Found services and characteristics");
  is_connected = true;

  notify(D_CONNECTED);
  return true;
}