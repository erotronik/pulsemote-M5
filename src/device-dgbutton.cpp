#include <NimBLEDevice.h>
#include <esp_log.h>

#include "comms-bt.hpp"
#include "device-dgbutton.hpp"
#include "device.hpp"
#include <functional>
#include <map>

NimBLEUUID dgbutton_SERVICE_BLEUUID("180c");
NimBLEUUID dgbutton_UUID_RX("150b");
NimBLEUUID dgbutton_UUID_TX("150a");

bool device_dgbutton::is_device(NimBLEAdvertisedDevice* advertisedDevice) {
   return (!strcmp(advertisedDevice->getName().c_str(), "47L120100"));
}

void device_dgbutton::set_callback(device_callback c) { update_callback = c; }

void device_dgbutton::notify(type_of_change change) {
  if (update_callback) update_callback(change, this);
}

void device_dgbutton::connected_callback() {
  ESP_LOGI(getShortName(), "Client onConnect");
}

void device_dgbutton::disconnected_callback(int reason) {
  is_connected = false;
  ESP_LOGI(getShortName(), "Client onDisconnect reason: %d", reason);
  notify(D_DISCONNECTED);
}

class DevicedgbuttonNimBLEClientCallback : public NimBLEClientCallbacks {
 public:
  DevicedgbuttonNimBLEClientCallback(device_dgbutton* instance) {
    device_dgbutton_instance = instance;
  }

  void onConnect(NimBLEClient* pclient){
    device_dgbutton_instance->connected_callback();
  }

  // arduino
  void onDisconnect(NimBLEClient* pclient) {
    device_dgbutton_instance->disconnected_callback(0);
  }

  // esp-idf
  void onDisconnect(NimBLEClient* pclient, int reason) {
    device_dgbutton_instance->disconnected_callback(reason);
  }

 private:
  device_dgbutton* device_dgbutton_instance;
};

device_dgbutton::device_dgbutton() {
  events = xQueueCreate(10,sizeof(int));
}

device_dgbutton::~device_dgbutton() {
  // bleClient->deleteServices(); // deletes all services, which should delete
  // all characteristics NimBLEDevice::deleteClient(bleClient); // will also
  // disconnect
}

void device_dgbutton::ble_mk_callback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData,
    size_t length, bool isNotify) {
      dgbutton_event event = dgbutton_event::NONE;

      if (length > 2 && pData[0] == 0x53 && pData[1] == 0x00) {  // rest is end of MAC
        state = dgbutton_state::SENDSTART;

        // please tell us when the button is pushed and released
        uint8_t a[] = { 0x50, 0x01, 
                        0x01, 0x01, 
                        0x00, // 01 = invert push/release
                        0x01, // 01 = send a 5c on held every second
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        device_dgbutton::tx_Characteristic->writeValue(a, sizeof(a));
      } else
      if (length > 2 && pData[0] == 0x51 && pData[1] == 0x01) { // confirmed command
        state = dgbutton_state::LISTEN;
        ESP_LOGI("dgcb","Battery level might be %d",pData[3]);
        battery = pData[3];
      } else
      if (length > 2 && pData[0] == 0x5a) {
        ESP_LOGI("dgcb","Push!");
        event = dgbutton_event::PUSH;
      } else
      if (length > 2 && pData[0] == 0x5c) {
        ESP_LOGI("dgcb","Held!");
        event = dgbutton_event::HELD;
      } else
      if (length > 2 && pData[0] == 0x5b) {
        ESP_LOGI("dgcb","Release!");
        event = dgbutton_event::RELEASE;
      } else {
        for (int i=0;i<length;i++) {
          ESP_LOGI("dgcb","%02x",pData[i]);
        }
      }
      if (event != dgbutton_event::NONE) {
        xQueueSend(events, &event, 0);
      }
    }


bool device_dgbutton::connect_to_device(NimBLEAdvertisedDevice* device) {
  ESP_LOGI(getShortName(), "Connecting");

  if (!bleClient) {
    bleClient = NimBLEDevice::createClient();
    bleClient->setClientCallbacks(new DevicedgbuttonNimBLEClientCallback(this));
  }
  notify(D_CONNECTING);
  bool res = true;

  ESP_LOGI(getShortName(), "Will try to connect to %s", device->getAddress().toString().c_str());

  if (!bleClient->connect(device)) {
    ESP_LOGE(getShortName(), "Connection failed");
    return false;
  }
  ESP_LOGI(getShortName(), "Connection established");
  res &= ble_get_service(dgbuttonService, bleClient, dgbutton_SERVICE_BLEUUID);
  if (res == false) {
    ESP_LOGE(getShortName(), "Missing service");
    bleClient->disconnect();
    return false;
  }
  state = dgbutton_state::WFHELLO;

  res &= ble_get_characteristic( dgbuttonService, tx_Characteristic, dgbutton_UUID_TX, nullptr);

  if (res == false) {
    ESP_LOGE(getShortName(), "Missing tx characteristic");
    bleClient->disconnect();
    return false;
  }

  res &= ble_get_characteristic_response(dgbuttonService, rx_Characteristic, dgbutton_UUID_RX,
      std::bind(&device_dgbutton::ble_mk_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

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