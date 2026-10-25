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

const char** device_lovense::get_patterns() {
    static const char* hush_patterns[5] = {
        "Constant", "Luck", "RampDown", "FastBuzz", "RampUp"
    };

    static const char* hush2_patterns[6] = {
        "Constant", "Luck", "FastBuzz", "Wave", "RampUp", "Heartbeat"
    };

    return model==LOVENSE_HUSH2 ? hush2_patterns : hush_patterns;
}

int device_lovense::get_patterns_n() {
    return  model==LOVENSE_HUSH2 ? 6 : 5;
}

bool device_lovense::is_device(const NimBLEAdvertisedDevice* advertisedDevice) {
  if (strnstr(advertisedDevice->getName().c_str(),"LVS-",4))
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
    ble_delete_client(bleClient);
  }
  if (notifyQueue) {
    vQueueDelete(notifyQueue);
    notifyQueue = nullptr;
  }
}

// mode 0 is continuous, mode 1 is randomly pick something, mode 2,3,4 are presets 1,2,3

void device_lovense::setmodespeed(int mode, int speed) {
  char buf[32];
  if (mode ==1 && speed !=0) {
    int i = rand()%(5+get_patterns_n()-2);
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

// Doesn't wait for the reply ("OK;"): waiting stalls the UI on every knob click
void device_lovense::ble_lovense_send(const char* newValue) {
  if (!is_connected) 
    return;
  ESP_LOGI("lovense","Sending %s" ,newValue);
  device_lovense::uuid_tx_Characteristic->writeValue(newValue, strlen(newValue));
}

int device_lovense::ble_lovense_getbattery() {
  if (!is_connected) 
    return 0;
  xQueueReset(notifyQueue);
  static const char* newValue = "Battery;";
  uuid_tx_Characteristic->writeValue(newValue, strlen(newValue), true);
  NotifyPacket received;

  // Replies to earlier commands can still turn up first: skip anything that isn't a number
  TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(1000);
  TickType_t now;
  while ((int32_t)(end - (now = xTaskGetTickCount())) > 0 && xQueueReceive(notifyQueue, &received, end - now)) {
    char buf[8] = {0};
    int len = received.length < 7 ? received.length : 7;

    memcpy(buf, received.data, len);
    buf[len] = 0;

    ESP_LOGI("lovense", "battery raw '%s' len=%d", buf, received.length);

    if (isdigit((unsigned char)buf[0]))
      return atoi(buf);
  }

  ESP_LOGW("lovense", "battery timeout");
  return 0;
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
  is_connected = false;
  ESP_LOGI(getShortName(), "Connecting");
  if (!notifyQueue)
    notifyQueue = xQueueCreate(NOTIFY_QUEUE_LEN, sizeof(NotifyPacket));

  if (!bleClient) {
    bleClient = NimBLEDevice::createClient();
    bleClient->setClientCallbacks(new DevicelovenseNimBLEClientCallback(this));
  }
  notify(D_CONNECTING);
  bool res = true;

  ESP_LOGI(getShortName(), "Will try to connect to %s",
         device->getAddress().toString().c_str());

vTaskDelay(pdMS_TO_TICKS(300));

  NimBLEUUID serviceUuid = lovense_SERVICE_BLEUUID;
  NimBLEUUID txUuid = lovense_UUID_TX;
  NimBLEUUID rxUuid = lovense_UUID_RX;

if (!bleClient->connect(device, true, false, false)) {
  ESP_LOGE(getShortName(), "Connection failed");
  return false;
}
  model = LOVENSE_UNKNOWN;
  ESP_LOGI(getShortName(), "Connection established");

  res = ble_get_service(lovenseService, bleClient, serviceUuid);
  if (res) model = LOVENSE_HUSH;
  if (!res) {
  for (uint8_t i = 0; i < device->getServiceUUIDCount(); i++) {
    std::string s = device->getServiceUUID(i).toString();

    if (s.length() == 36 &&
        s.substr(2, 6) == "300001" &&
        s.substr(9, 3) == "002" &&
        s.substr(13) == "-4bd4-bbd5-a6920e4c5653") {
        
      model = LOVENSE_HUSH2;
      std::string rx = s;
      std::string tx = s;
      rx.replace(4, 4, "0003");
      tx.replace(4, 4, "0002");

      serviceUuid = NimBLEUUID(s);
      rxUuid = NimBLEUUID(rx);
      txUuid = NimBLEUUID(tx);

      ESP_LOGI(getShortName(), "Trying v3 service %s", s.c_str());
      ESP_LOGI(getShortName(), "Trying v3 rx %s", rx.c_str());
      ESP_LOGI(getShortName(), "Trying v3 tx %s", tx.c_str());

      res = ble_get_service(lovenseService, bleClient, serviceUuid);
      break;
    }
  }
}
if (!res) {
  ESP_LOGE(getShortName(), "Missing service");
  bleClient->disconnect();
  return false;
}

  res = ble_get_characteristic(lovenseService, uuid_rx_Characteristic, rxUuid,
      std::bind(&device_lovense::ble_mk_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), true);
  if (!res) {
    ESP_LOGE(getShortName(), "Missing rx characteristic");
    bleClient->disconnect();
    return false;
  }
  res = ble_get_characteristic(lovenseService, uuid_tx_Characteristic, txUuid, nullptr);

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