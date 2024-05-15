#include <NimBLEDevice.h>
#include <esp_log.h>

#include <device-mk312.hpp>
#include <device.hpp>
#include <functional>
#include <map>

NimBLEUUID MK312_SERVICE_BLEUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
NimBLEUUID MK312_UUID_RXTX("0000ffe1-0000-1000-8000-00805f9b34fb");

bool device_mk312::is_device(NimBLEAdvertisedDevice* advertisedDevice) {
  return (advertisedDevice->isAdvertisingService(MK312_SERVICE_BLEUUID));
}

bool device_mk312::get_isconnected() { return is_connected; }

void device_mk312::set_callback(device_callback c) { update_callback = c; }

void device_mk312::notify(type_of_change change) {
  if (update_callback) update_callback(change, this);
}

void device_mk312::connected_callback() {
  ESP_LOGI(getShortName(), "Client onConnect");
}

void device_mk312::disconnected_callback(int reason) {
  is_connected = false;
  ESP_LOGI(getShortName(), "Client onDisconnect reason: %d", reason);
  notify(D_DISCONNECTED);
}

class DeviceMK312NimBLEClientCallback : public NimBLEClientCallbacks {
 public:
  DeviceMK312NimBLEClientCallback(device_mk312* instance) {
    device_mk312_instance = instance;
  }

  void onConnect(NimBLEClient* pclient) {
    device_mk312_instance->connected_callback();
  }

  // arduino
  void onDisconnect(NimBLEClient* pclient) {
    device_mk312_instance->disconnected_callback(0);
  }

  // esp-idf
  void onDisconnect(NimBLEClient* pclient, int reason) {
    device_mk312_instance->disconnected_callback(reason);
  }

 private:
  device_mk312* device_mk312_instance;
};

bool device_mk312::getService(NimBLERemoteService*& service, NimBLEUUID uuid) {
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

device_mk312::device_mk312() {}

device_mk312::~device_mk312() {
  // bleClient->deleteServices(); // deletes all services, which should delete
  // all characteristics NimBLEDevice::deleteClient(bleClient); // will also
  // disconnect
}

void device_mk312::etbox_flushcb(void) {
  device_mk312::uuid_rxtx_Characteristic->writeValue(mktx, mktx_n);
  mktx_n = 0;
}

void device_mk312::etbox_txcb(byte c) {
  mktx[mktx_n++] = c;
  if (mktx_n == mktx_maxlen) etbox_flushcb();
}

// circular buffer with a timeout! wow ok.

int device_mk312::etbox_rxcb(char* p, int x) {
  unsigned long timeout = millis() + 200;
  byte by = 0;
  while (by < x && millis() < timeout) {
    if (mkwptr != mkrptr) {
      if (mkrptr == mkbuffer_maxlen) mkrptr = 0;
      p[by++] = mkbuffer[mkrptr++];
      timeout = millis() + 5;
    } else {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
  return by;
}

void device_mk312::set_mode(int p) {
  BOX.setbyte(ETMEM_mode, p - 1);
  BOX.setbyte(ETMEM_pushbutton, ETBUTTON_setmode);
  vTaskDelay(pdMS_TO_TICKS(180));
  BOX.setbyte(ETMEM_pushbutton, ETBUTTON_lockmode);
  vTaskDelay(pdMS_TO_TICKS(180));
}

int device_mk312::get_mode() {
  int mode = BOX.getbyte(ETMEM_mode);
  return (mode > 0x76 ? mode - 0x76 : 0);
}

void device_mk312::etbox_on(byte mode) {
  if (BOX.isconnected()) {
    if (mode == 0) mode = BOX.getbyte(ETMEM_mode);
    set_mode(mode);
  }
}
void device_mk312::etbox_off(void) {
  if (BOX.isconnected()) {
    BOX.setbyte(ETMEM_pushbutton, 0x18);
    BOX.setbyte(0x4180, 0x64);  // blank the program display part
    BOX.setbyte(ETMEM_pushbutton, 0x15);
    BOX.getbyte(ETMEM_pushbutton);
  }
}
void device_mk312::etbox_setbyte(word address, byte data) {
  if (BOX.isconnected()) {
    BOX.setbyte(address, data);
  }
}

byte device_mk312::etbox_getbyte(word address) {
  if (BOX.isconnected()) return BOX.getbyte(address);
  return 0;
}

void device_mk312::ble_mk_callback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData,
    size_t length, bool isNotify) {
  for (byte i = 0; i < length; i++) {
    if (mkwptr == mkbuffer_maxlen) mkwptr = 0;
    mkbuffer[mkwptr++] = (byte)(*(pData + i));
  }
}

bool device_mk312::connect_to_device(NimBLEAdvertisedDevice* device) {
  ESP_LOGI(getShortName(), "Connecting \n");

  if (!bleClient) {
    bleClient = NimBLEDevice::createClient();
    bleClient->setClientCallbacks(new DeviceMK312NimBLEClientCallback(this));
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
  res &= getService(mk312Service, MK312_SERVICE_BLEUUID);
  if (res == false) {
    ESP_LOGE(getShortName(), "Missing service");
    bleClient->disconnect();
    return false;
  }

  res &= getCharacteristic(
      mk312Service, uuid_rxtx_Characteristic, MK312_UUID_RXTX,
      std::bind(&device_mk312::ble_mk_callback, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3,
                std::placeholders::_4));

  if (res == false) {
    ESP_LOGE(getShortName(), "Missing characteristic");
    bleClient->disconnect();
    return false;
  }
  ESP_LOGI(getShortName(), "Found services and characteristics");
  is_connected = true;

  BOX.begin(std::bind(&device_mk312::etbox_txcb, this, std::placeholders::_1),
            std::bind(&device_mk312::etbox_rxcb, this, std::placeholders::_1,
                      std::placeholders::_2),
            std::bind(&device_mk312::etbox_flushcb, this));
  BOX.setdebug(Serial, 0);
  BOX.newhello();
  if (!BOX.isconnected()) {
    ESP_LOGE(getShortName(), "couldnt do hello handshake to box");
    bleClient->disconnect();
    NimBLEDevice::deleteClient(bleClient);
    notify(D_DISCONNECTED);
    return false;
  }
  int y = BOX.getbyte(ETMEM_knoba);
  if (y == -1) {
    ESP_LOGE(getShortName(), "couldnt get data from box");
    bleClient->disconnect();
    NimBLEDevice::deleteClient(bleClient);
    notify(D_DISCONNECTED);
    return false;
  }
  ESP_LOGI(getShortName(), "%s KNOBA=%d\n", getShortName(), y);
  notify(D_CONNECTED);
  return true;
}