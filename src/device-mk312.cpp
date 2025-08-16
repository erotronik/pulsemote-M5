#include <NimBLEDevice.h>
#include <esp_log.h>

#include "comms-bt.hpp"
#include <device-mk312.hpp>
#include <device.hpp>
#include <functional>
#include <map>

// This code works with a MK-312BT box that has a bluetooth serial BLE board installed
// or with an original ET-312B that has a bluetooth serial dongle attached. In both
// cases the name of the device must be "MK" or have a "312" inside it.
//
// I connected a cheap BT578v3 to an ET-312B. To start you need to connect it to a PC
// and issue a command like AT+NAMBET312\r\n to set the name and AT+BAUD5 to set it to
// 19200 (it defaults to 9600) see http://www.irxon.com/pdf/docs/BT578_V3_EN.pdf

NimBLEUUID MK312_SERVICE_BLEUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
NimBLEUUID MK312_UUID_RXTX("0000ffe1-0000-1000-8000-00805f9b34fb");

bool device_mk312::is_device(NimBLEAdvertisedDevice* advertisedDevice) {
  if (advertisedDevice->isAdvertisingService(MK312_SERVICE_BLEUUID)) {
    // any BLE UART will match this so only try to connect to something with name MK or 312 in the name
    if (strstr(advertisedDevice->getName().c_str(),"312")) return true;
    if (!strcmp(advertisedDevice->getName().c_str(),"MK")) return true;
  }
  return false;
}

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

device_mk312::device_mk312() {}

device_mk312::~device_mk312() {
  // bleClient->deleteServices(); // deletes all services, which should delete
  // all characteristics NimBLEDevice::deleteClient(bleClient); // will also
  // disconnect
}

void device_mk312::etbox_flushcb(void) {
  ESP_LOGD("mk312","bt write %d bytes core%d",mktx_n, xPortGetCoreID());
  device_mk312::uuid_rxtx_Characteristic->writeValue(mktx, mktx_n);
  mktx_n = 0;
}

void device_mk312::etbox_txcb(byte c) {
  mktx[mktx_n++] = c;
  if (mktx_n == mktx_maxlen) etbox_flushcb();
}

int device_mk312::etbox_rxcb(char* p, int x) {
  NotifyPacket received;

  if (xQueueReceive(notifyQueue, &received, pdMS_TO_TICKS(200))) {
    int copyLen = (received.length < x) ? received.length : x;
    memcpy(p, received.data, copyLen);
    return copyLen;
  }
  return 0;
}

bool device_mk312::connected() {
  if (!is_connected) return false;
  return BOX.isconnected();
}

void device_mk312::etbox_setbyte(word address, byte data) {
  if (BOX.isconnected()) BOX.setbyte(address, data);
}

byte device_mk312::etbox_getbyte(word address) {
  if (BOX.isconnected()) return BOX.getbyte(address);
  return 0;
}

void device_mk312::set_mode(int p) {
  int q = p + ETMODE_waves;
  if (p == etmodes_potluck) 
    q = potluck[random(0,potluck_n)];
  ESP_LOGD("set_mode","set mode %d",q);
  etbox_setbyte(ETMEM_modesplit, q );
  etbox_setbyte(ETMEM_runcommand, ETCOMMAND_SELECTNEWMODE);
  etbox_setbyte(ETMEM_runcommand2, ETCOMMAND_EXITMENU); // redraw

  vTaskDelay(pdMS_TO_TICKS(180));
  lastvalidmode = p + ETMODE_waves;
}

void device_mk312::next_mode() {
  etbox_setbyte(ETMEM_runcommand, ETCOMMAND_SWITCHTONEXTMODE);
  get_mode();
}

int device_mk312::get_last_mode() {
  return (lastvalidmode > ETMODE_waves? (lastvalidmode - ETMODE_waves):0);
}

int device_mk312::get_mode() {
  int mode = etbox_getbyte(ETMEM_mode);
  ESP_LOGD("get_mode","get mode %d",mode);
  if (mode != -1) lastvalidmode = mode;
  return (lastvalidmode > ETMODE_waves? (lastvalidmode - ETMODE_waves):0);
}

void device_mk312::etbox_on(int mode) {
  if (mode == -1) mode = get_mode();
  set_mode(mode);
  lastvalidmode = mode+ETMODE_waves;
}

void device_mk312::etbox_off(void) {
  etbox_setbyte(ETMEM_runcommand, ETCOMMAND_INITMODULE);
  etbox_setbyte(ETMEM_runcommanddata, 0x64);  // 8 spaces over the mode
  etbox_setbyte(ETMEM_runcommand2, ETCOMMAND_LCDWRITESTRING);
}

void device_mk312::etbox_setlevela(byte data) {
  etbox_setbyte(ETMEM_knoba, (data * 256+99) / 100); // Round up to match the display
}

void device_mk312::etbox_setlevelb(byte data) {
  etbox_setbyte(ETMEM_knobb, (data * 256+99) / 100); // Round up to match the display
}

void device_mk312::etbox_setpanellock(bool x) {
  etbox_setbyte(ETMEM_panellock, x?1:0);
}

void device_mk312::ble_mk_callback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData,
    size_t length, bool isNotify) {
  ESP_LOGD("mk312","ble callback from core%d",xPortGetCoreID());
  NotifyPacket packet;
  packet.length = length > NOTIFY_MAX_DATA ? NOTIFY_MAX_DATA : length;
  memcpy(packet.data, pData, packet.length);

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(notifyQueue, &packet, &xHigherPriorityTaskWoken);

  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

bool device_mk312::connect_to_device(NimBLEAdvertisedDevice* device) {
  notifyQueue = xQueueCreate(NOTIFY_QUEUE_LEN, sizeof(NotifyPacket));

  ESP_LOGI(getShortName(), "Connecting core%d", xPortGetCoreID());

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
  
  res &= ble_get_service(mk312Service, bleClient, MK312_SERVICE_BLEUUID);
  if (res == false) {
    ESP_LOGE(getShortName(), "Missing service");
    bleClient->disconnect();
    return false;
  }

  res &= ble_get_characteristic(mk312Service, uuid_rxtx_Characteristic, MK312_UUID_RXTX,
      std::bind(&device_mk312::ble_mk_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

  if (res == false) {
    ESP_LOGE(getShortName(), "Missing characteristic");
    bleClient->disconnect();
    return false;
  }
  ESP_LOGI(getShortName(), "Found services and characteristics");
  is_connected = true;

  BOX.begin(std::bind(&device_mk312::etbox_txcb, this, std::placeholders::_1),
            std::bind(&device_mk312::etbox_rxcb, this, std::placeholders::_1, std::placeholders::_2),
            std::bind(&device_mk312::etbox_flushcb, this));
  BOX.setdebug(Serial, 2);
  BOX.newhello();
  if (!BOX.isconnected()) {
    ESP_LOGE(getShortName(), "couldnt do hello handshake to box");
    //bleClient->disconnect();
    NimBLEDevice::deleteClient(bleClient);
    bleClient = NULL;
    notify(D_DISCONNECTED);
    return false;
  }
  int y = BOX.getbyte(ETMEM_knoba); // any location really just to ensure the handshake worked
  if (y == -1) {
    ESP_LOGE(getShortName(), "couldnt get data from box");
    //bleClient->disconnect();
    NimBLEDevice::deleteClient(bleClient);
    bleClient = NULL;
    notify(D_DISCONNECTED);
    return false;
  }
  ESP_LOGI(getShortName(), "%s KNOBA=%d", getShortName(), y);
  notify(D_CONNECTED);
  return true;
}
