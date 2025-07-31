#include <NimBLEDevice.h>
#include <esp_log.h>

#include "device-lovense.hpp"
#include "comms-bt.hpp"
#include "device.hpp"
#include <functional>
#include <map>
#include "lvgl-utils.h" // for printf_log()


// Lovense Hush for now

NimBLEUUID lovense_SERVICE_BLEUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
NimBLEUUID lovense_UUID_RX("6e400003-b5a3-f393-e0a9-e50e24dcca9e");
NimBLEUUID lovense_UUID_TX("6e400002-b5a3-f393-e0a9-e50e24dcca9e");

bool device_lovense::is_device(NimBLEAdvertisedDevice* advertisedDevice) {
   if (advertisedDevice->isAdvertisingService(lovense_SERVICE_BLEUUID))
    if (strnstr(advertisedDevice->getName().c_str(),"LVS-Z",5))
        return true;
   return false;
}

void device_lovense::set_callback(device_callback c) { update_callback = c; }

void device_lovense::notify(type_of_change change) {
  if (update_callback) update_callback(change, this);
}

void device_lovense::connected_callback() {
  ESP_LOGI(getShortName(), "Client onConnect");
}

void device_lovense::disconnected_callback(int reason) {
  is_connected = false;
  ESP_LOGI(getShortName(), "Client onDisconnect reason: %d", reason);
  notify(D_DISCONNECTED);
}

class DevicelovenseNimBLEClientCallback : public NimBLEClientCallbacks {
 public:
  DevicelovenseNimBLEClientCallback(device_lovense* instance) {
    device_lovense_instance = instance;
  }

  void onConnect(NimBLEClient* pclient) {
    device_lovense_instance->connected_callback();
  }

  // arduino
  void onDisconnect(NimBLEClient* pclient) {
    device_lovense_instance->disconnected_callback(0);
  }

  // esp-idf
  void onDisconnect(NimBLEClient* pclient, int reason) {
    device_lovense_instance->disconnected_callback(reason);
  }

 private:
  device_lovense* device_lovense_instance;
};

device_lovense::device_lovense() {}

device_lovense::~device_lovense() {
  // bleClient->deleteServices(); // deletes all services, which should delete
  // all characteristics NimBLEDevice::deleteClient(bleClient); // will also
  // disconnect
}

// ble_lovense_send("Vibrate:0;"); to 20
// 1 ramp down
// 2 fast onoff to 10 probably
// https://docs.buttplug.io/docs/stpihkal/protocols/lovense/

void device_lovense::setmodespeed(int mode, int speed) {
  if (mode ==0 || speed ==0) {
    ble_lovense_send("Vibrate:" +String(speed)+ ";");
  } else {
    ble_lovense_send("Preset:" + String(mode) +";");
  }
}

void device_lovense::ble_lovense_send(String newValue) {
  if (is_connected) {
    xQueueReset(notifyQueue);
    ESP_LOGI("lovense","Sending %s" ,newValue);
    device_lovense::uuid_tx_Characteristic->writeValue(newValue.c_str(), newValue.length());
  } else {
    ESP_LOGE("lovense","cant send not connected");
    return;
  }
  NotifyPacket received;

  if (xQueueReceive(notifyQueue, &received, pdMS_TO_TICKS(200))) {
    char buf[100];
    strncat(buf,(const char *)received.data,received.length);
    buf[received.length]=0;
    ESP_LOGI("lovense","command returned %s",buf);
  }
}

int device_lovense::ble_lovense_getbattery() {
  if (!is_connected) 
    return 0;
  int batterylevel = 0;
  xQueueReset(notifyQueue);
  static const String newValue = "Battery;";
  device_lovense::uuid_tx_Characteristic->writeValue(newValue.c_str(), newValue.length());

  NotifyPacket received;

  if (xQueueReceive(notifyQueue, &received, pdMS_TO_TICKS(200))) {
    if (received.length == 4) {
      batterylevel = 100*(received.data[0]-'0')+10*(received.data[1]-'0')+received.data[0]-'0';
    }
  }
  return batterylevel;
}

void device_lovense::ble_mk_callback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData,
    size_t length, bool isNotify) {
  ESP_LOGD("lovense","ble callback from core%d",xPortGetCoreID());
  NotifyPacket packet;
  packet.length = length > NOTIFY_MAX_DATA ? NOTIFY_MAX_DATA : length;
  memcpy(packet.data, pData, packet.length);

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(notifyQueue, &packet, &xHigherPriorityTaskWoken);

  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

bool device_lovense::connect_to_device(NimBLEAdvertisedDevice* device) {
  ESP_LOGI(getShortName(), "Connecting");
  notifyQueue = xQueueCreate(NOTIFY_QUEUE_LEN, sizeof(NotifyPacket));

  if (!bleClient) {
    bleClient = NimBLEDevice::createClient();
    bleClient->setClientCallbacks(new DevicelovenseNimBLEClientCallback(this));
  }
  notify(D_CONNECTING);
  bool res = true;

  ESP_LOGI(getShortName(), "Will try to connect to %s",
           device->getAddress().toString().c_str());

  if (!bleClient->connect(device)) {
    ESP_LOGE(getShortName(), "Connection failed");
    return false;
  }
  ESP_LOGI(getShortName(), "Connection established");
  res &= ble_get_service(thrustService, bleClient, lovense_SERVICE_BLEUUID);
  if (res == false) {
    ESP_LOGE(getShortName(), "Missing service");
    bleClient->disconnect();
    return false;
  }

  res &= ble_get_characteristic(thrustService, uuid_rx_Characteristic, lovense_UUID_RX,
      std::bind(&device_lovense::ble_mk_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

  if (res == false) {
    ESP_LOGE(getShortName(), "Missing rx characteristic");
    bleClient->disconnect();
    return false;
  }
  res &= ble_get_characteristic(thrustService, uuid_tx_Characteristic, lovense_UUID_TX, nullptr);

  if (res == false) {
    ESP_LOGE(getShortName(), "Missing tx characteristic");
    bleClient->disconnect();
    return false;
  }

  ESP_LOGI(getShortName(), "Found services and characteristics");
  is_connected = true;

  //printf_log("lovense battery %d\n", ble_lovense_getbattery());

  //ble_lovense_send("Vibrate:0;");
  // 1 ramp down
  // 2 fast onoff to 10 probably

  notify(D_CONNECTED);
  return true;
}