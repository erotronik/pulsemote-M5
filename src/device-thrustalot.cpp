#include <NimBLEDevice.h>
#include <esp_log.h>

#include <device-thrustalot.hpp>
#include <device.hpp>
#include <functional>
#include <map>

NimBLEUUID THRUSTALOT_SERVICE_BLEUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
NimBLEUUID THRUSTALOT_UUID_RX("6e400003-b5a3-f393-e0a9-e50e24dcca9e");
NimBLEUUID THRUSTALOT_UUID_TX("6e400002-b5a3-f393-e0a9-e50e24dcca9e");

bool device_thrustalot::is_device(NimBLEAdvertisedDevice* advertisedDevice) {
   if (advertisedDevice->isAdvertisingService(THRUSTALOT_SERVICE_BLEUUID))
    if (strstr(advertisedDevice->getName().c_str(),"Thrustalot"))
        return true;
   return false;
}

bool device_thrustalot::get_isconnected() { return is_connected; }

void device_thrustalot::set_callback(device_callback c) { update_callback = c; }

void device_thrustalot::notify(type_of_change change) {
  if (update_callback) update_callback(change, this);
}

void device_thrustalot::connected_callback() {
  ESP_LOGI(getShortName(), "Client onConnect");
}

void device_thrustalot::disconnected_callback(int reason) {
  is_connected = false;
  ESP_LOGI(getShortName(), "Client onDisconnect reason: %d", reason);
  notify(D_DISCONNECTED);
}

class DeviceThrustalotNimBLEClientCallback : public NimBLEClientCallbacks {
 public:
  DeviceThrustalotNimBLEClientCallback(device_thrustalot* instance) {
    device_thrustalot_instance = instance;
  }

  void onConnect(NimBLEClient* pclient) {
    device_thrustalot_instance->connected_callback();
  }

  // arduino
  void onDisconnect(NimBLEClient* pclient) {
    device_thrustalot_instance->disconnected_callback(0);
  }

  // esp-idf
  void onDisconnect(NimBLEClient* pclient, int reason) {
    device_thrustalot_instance->disconnected_callback(reason);
  }

 private:
  device_thrustalot* device_thrustalot_instance;
};

bool device_thrustalot::getService(NimBLERemoteService*& service, NimBLEUUID uuid) {
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

device_thrustalot::device_thrustalot() {}

device_thrustalot::~device_thrustalot() {
  // bleClient->deleteServices(); // deletes all services, which should delete
  // all characteristics NimBLEDevice::deleteClient(bleClient); // will also
  // disconnect
}


void device_thrustalot::ble_thrustalot_send(String newValue) {
  if (is_connected) {
    ESP_LOGI("Thrustalot","Sending %s" ,newValue);
    device_thrustalot::uuid_tx_Characteristic->writeValue(newValue.c_str(), newValue.length());
  } else 
    ESP_LOGE("Thrustalot","cant send not connected");
}

void device_thrustalot::thrustallthewayin(void) {
  ble_thrustalot_send("!B711X"); // to maximum position!
}
void device_thrustalot::thrustallthewayout(void) {
  ble_thrustalot_send("!B811X"); // to minimum position!

}
void device_thrustalot::thrustonetime(int speed) {
  char speeds[20];
  sprintf(speeds, "!U%03dX", map(speed, 0, 100, 45, 255));
  ble_thrustalot_send(speeds);  
}

void device_thrustalot::ble_mk_callback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData,
    size_t length, bool isNotify) {
  char bug[100];
  byte j = 0;
  for (byte i = 0; i < length; i++) {
    bug[j] = (char)(*(pData + i));
    if (bug[j] == '\n' || j == length - 1 || j == 98) {
      bug[j + 1] = 0;
      if (bug[0] == 'I') {
        thrustcb_pos = 2;
      } else if (bug[0] == 'A') {
        thrustcb_pos = 1;
        String x = String(bug + 1);
        Serial.println(x.toInt());
        thrustcb_count = x.toInt();
      } else if (bug[0] == 'S') {
        if (bug[1] != '0') {
          //thrustcb_pos = 0;
        } else {
          thrustcb_left = 0;
        }
      } else if (bug[0] == 'T') {
        String x = String(bug + 1);
        Serial.println(x.toInt());
        thrustcb_left = x.toInt();
      }
      //ESP_LOGI("thrusthardware","%s",bug);
      j = 0;
    } else {
      j++;
    }
  }
}

bool device_thrustalot::connect_to_device(NimBLEAdvertisedDevice* device) {
  ESP_LOGI(getShortName(), "Connecting \n");

  if (!bleClient) {
    bleClient = NimBLEDevice::createClient();
    bleClient->setClientCallbacks(new DeviceThrustalotNimBLEClientCallback(this));
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
  res &= getService(thrustService, THRUSTALOT_SERVICE_BLEUUID);
  if (res == false) {
    ESP_LOGE(getShortName(), "Missing service");
    bleClient->disconnect();
    return false;
  }


  res &= getCharacteristic(
      thrustService, uuid_rx_Characteristic, THRUSTALOT_UUID_RX,
      std::bind(&device_thrustalot::ble_mk_callback, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3,
                std::placeholders::_4));

  if (res == false) {
    ESP_LOGE(getShortName(), "Missing rx characteristic");
    bleClient->disconnect();
    return false;
  }
  res &= getCharacteristic(
      thrustService, uuid_tx_Characteristic, THRUSTALOT_UUID_TX, nullptr);

  if (res == false) {
    ESP_LOGE(getShortName(), "Missing tx characteristic");
    bleClient->disconnect();
    return false;
  }

  ESP_LOGI(getShortName(), "Found services and characteristics");
  is_connected = true;

  notify(D_CONNECTED);
  return true;
}