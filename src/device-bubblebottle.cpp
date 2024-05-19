#include <NimBLEDevice.h>
#include <esp_log.h>

#include <device-bubblebottle.hpp>
#include <device.hpp>
#include <functional>
#include <map>

NimBLEUUID BUBBLEBOTTLE_SERVICE_BLEUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
NimBLEUUID BUBBLEBOTTLE_UUID_RX("6e400003-b5a3-f393-e0a9-e50e24dcca9e");
NimBLEUUID BUBBLEBOTTLE_UUID_TX("6e400002-b5a3-f393-e0a9-e50e24dcca9e");

bool device_bubblebottle::is_device(NimBLEAdvertisedDevice* advertisedDevice) {
   if (advertisedDevice->isAdvertisingService(BUBBLEBOTTLE_SERVICE_BLEUUID)) {
    uint8_t *md = (uint8_t *)advertisedDevice->getManufacturerData().data();
    if (md && md[0] == 0xf1 && md[1] == 0xf1) 
      return true;
   }
   return false;
}

bool device_bubblebottle::get_isconnected() { return is_connected; }

void device_bubblebottle::set_callback(device_callback c) { update_callback = c; }

void device_bubblebottle::notify(type_of_change change) {
  if (update_callback) update_callback(change, this);
}

void device_bubblebottle::connected_callback() {
  ESP_LOGI(getShortName(), "Client onConnect");
}

void device_bubblebottle::disconnected_callback(int reason) {
  is_connected = false;
  ESP_LOGI(getShortName(), "Client onDisconnect reason: %d", reason);
  notify(D_DISCONNECTED);
}

class DeviceBubblebottleNimBLEClientCallback : public NimBLEClientCallbacks {
 public:
  DeviceBubblebottleNimBLEClientCallback(device_bubblebottle* instance) {
    device_bubblebottle_instance = instance;
  }

  void onConnect(NimBLEClient* pclient){
    device_bubblebottle_instance->connected_callback();
  }

  // arduino
  void onDisconnect(NimBLEClient* pclient) {
    device_bubblebottle_instance->disconnected_callback(0);
  }

  // esp-idf
  void onDisconnect(NimBLEClient* pclient, int reason) {
    device_bubblebottle_instance->disconnected_callback(reason);
  }

 private:
  device_bubblebottle* device_bubblebottle_instance;
};

bool device_bubblebottle::getService(NimBLERemoteService*& service, NimBLEUUID uuid) {
  ESP_LOGD(getShortName(), "Getting service %s", uuid.toString().c_str());
  service = bleClient->getService(uuid);
  if (service == nullptr) {
    ESP_LOGE(getShortName(), "Failed to find service UUID: %s",
             uuid.toString().c_str());
    return false;
  }
  return true;
}

bool getCharacteristic(NimBLERemoteService* service,
                       NimBLERemoteCharacteristic*& c, NimBLEUUID uuid,
                       notify_callback notifyCallback);

device_bubblebottle::device_bubblebottle() {
  events = xQueueCreate(10,sizeof(int));
}

device_bubblebottle::~device_bubblebottle() {
  // bleClient->deleteServices(); // deletes all services, which should delete
  // all characteristics NimBLEDevice::deleteClient(bleClient); // will also
  // disconnect
}

void device_bubblebottle::ble_bubblebottle_send(String newValue) {
  if (is_connected) {
    ESP_LOGI("bubblebottle","Sending %s" ,newValue);
    device_bubblebottle::uuid_tx_Characteristic->writeValue(newValue.c_str(), newValue.length());
  } else 
    ESP_LOGE("bubblebottle","cant send not connected");
}

void device_bubblebottle::ble_mk_callback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData,
    size_t length, bool isNotify) {
  char bug[100];
  byte j = 0;
  for (byte i = 0; i < length; i++) {
    bug[j] = (char)(*(pData + i));
    if (bug[j] == '\n' || j == length - 1 || j == 98)  {
      bug[j + 1] = 0;
      if (bug[0] == '*')  {
        ESP_LOGD("bottlecb","%c", bug[1]);
        if (bug[1] == 'B')
          bottle_state = 1;
        else if (bug[1] == 'R') 
          bottle_state = 2;
        xQueueSend(events, &bottle_state, 0);
      } 
      j = 0;
    } else
      j++;
  }
}

bool device_bubblebottle::connect_to_device(NimBLEAdvertisedDevice* device) {
  ESP_LOGI(getShortName(), "Connecting \n");

  if (!bleClient) {
    bleClient = NimBLEDevice::createClient();
    bleClient->setClientCallbacks(new DeviceBubblebottleNimBLEClientCallback(this));
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
  res &= getService(bubblebottleService, BUBBLEBOTTLE_SERVICE_BLEUUID);
  if (res == false) {
    ESP_LOGE(getShortName(), "Missing service");
    bleClient->disconnect();
    return false;
  }


  res &= getCharacteristic(
      bubblebottleService, uuid_rx_Characteristic, BUBBLEBOTTLE_UUID_RX,
      std::bind(&device_bubblebottle::ble_mk_callback, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3,
                std::placeholders::_4));

  if (res == false) {
    ESP_LOGE(getShortName(), "Missing rx characteristic");
    bleClient->disconnect();
    return false;
  }
  // don't really need to transmit to it, so don't bother with the tx characteristic
  #if 0
  res &= getCharacteristic(
      bubblebottleService, uuid_tx_Characteristic, BUBBLEBOTTLE_UUID_TX, nullptr);

  if (res == false) {
    ESP_LOGE(getShortName(), "Missing tx characteristic");
    bleClient->disconnect();
    return false;
  }
  #endif

  ESP_LOGI(getShortName(), "Found services and characteristics");
  is_connected = true;

  notify(D_CONNECTED);
  return true;
}