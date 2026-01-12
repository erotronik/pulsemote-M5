#include <NimBLEDevice.h>
#include <esp_log.h>

#include "device-lovense.hpp"
#include "comms-bt.hpp"
#include "device.hpp"
#include <functional>

// Lovense Hush for now
// https://docs.buttplug.io/docs/stpihkal/protocols/lovense/

NimBLEUUID lovense_SERVICE_BLEUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
NimBLEUUID lovense_UUID_RX("6e400003-b5a3-f393-e0a9-e50e24dcca9e");
NimBLEUUID lovense_UUID_TX("6e400002-b5a3-f393-e0a9-e50e24dcca9e");

bool device_lovense::is_device(const NimBLEAdvertisedDevice* advertisedDevice) {
  if (advertisedDevice->isAdvertisingService(lovense_SERVICE_BLEUUID)) // it's a pretty generic UART though so check the name too
    if (strnstr(advertisedDevice->getName().c_str(),"LVS-Z",5))
      return true;
  return false;
}

void device_lovense::set_callback(device_callback c) { 
  update_callback = c; 
}

void device_lovense::notify(type_of_change change) {
  if (update_callback) update_callback(change, this);
}

void device_lovense::connected_callback() {
  ESP_LOGI(getShortName(), "Client onConnect");
}

void device_lovense::disconnected_callback(int reason) {
  ESP_LOGI(getShortName(), "Client onDisconnect reason: %d", reason);
  is_connected = false;
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
  if (bleClient) {
    NimBLEDevice::deleteClient(bleClient);
    bleClient = nullptr;
  }
}

// mode 0 is continuous, mode 1 is randomly pick something, mode 2,3,4 are presets 1,2,3

void device_lovense::setmodespeed(int mode, int speed) {
  char buf[32];
  if (mode ==1 && speed !=0) {
    int i = rand()%(5+patterns_n-2);
    if (i<5) {
      mode = 0;
      speed = (i+1)*4;
    } else {
      mode = i-3;
    }
    ESP_LOGI("lovense","random mode=%d speed=%d",mode,speed);
  }
  if (mode ==0 || speed ==0) {
    snprintf(buf, sizeof(buf), "Vibrate:%d;", speed);
    ble_lovense_send(buf);
  } else  {
    snprintf(buf, sizeof(buf), "Preset:%d;", mode-1);
    ble_lovense_send(buf);
  }   
}

void device_lovense::ble_lovense_send(const char* newValue) {
  if (!is_connected) 
    return;
  xQueueReset(notifyQueue);
  ESP_LOGI("lovense","Sending %s" ,newValue);
  device_lovense::uuid_tx_Characteristic->writeValue(newValue, strlen(newValue));
  NotifyPacket received;

  if (xQueueReceive(notifyQueue, &received, pdMS_TO_TICKS(200))) {
    char buf[100];
    int len = received.length < 99? received.length: 99;
    strncat(buf,(const char *)received.data,len);
    buf[len]=0;
    ESP_LOGI("lovense","command returned %s",buf);
  }
}

int device_lovense::ble_lovense_getbattery() {
  if (!is_connected) 
    return 0;
  int batterylevel = 0;
  xQueueReset(notifyQueue);
  static const char* newValue = "Battery;";
  device_lovense::uuid_tx_Characteristic->writeValue(newValue, strlen(newValue));

  NotifyPacket received;

  if (xQueueReceive(notifyQueue, &received, pdMS_TO_TICKS(200))) {
    if (received.length == 4) {
      batterylevel = 100*(received.data[0]-'0')+10*(received.data[1]-'0')+received.data[0]-'0';
    }
  }
  return batterylevel;
}

void device_lovense::ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
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

  ESP_LOGI(getShortName(), "Will try to connect to %s", device->getAddress().toString().c_str());

  if (!bleClient->connect(device)) {
    ESP_LOGE(getShortName(), "Connection failed");
    return false;
  }
  ESP_LOGI(getShortName(), "Connection established");
  res = ble_get_service(lovenseService, bleClient, lovense_SERVICE_BLEUUID);
  if (!res) {
    ESP_LOGE(getShortName(), "Missing service");
    bleClient->disconnect();
    return false;
  }

  res = ble_get_characteristic(lovenseService, uuid_rx_Characteristic, lovense_UUID_RX,
      std::bind(&device_lovense::ble_mk_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
  if (!res) {
    ESP_LOGE(getShortName(), "Missing rx characteristic");
    bleClient->disconnect();
    return false;
  }
  res = ble_get_characteristic(lovenseService, uuid_tx_Characteristic, lovense_UUID_TX, nullptr);

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