#include <NimBLEDevice.h>
#include <esp_log.h>

#include "device-coyote2.hpp"
#include "device-mk312.hpp"
#include "device-thrustalot.hpp"
#include "device-bubblebottle.hpp"
#include "device-loop.hpp"
#include "device-dgbutton.hpp"
#include "device.hpp"

// An instance of each device is used for scanning
std::vector<Device*> ble_devices = { new device_loop(), new device_mk312(), new device_thrustalot(), new device_bubblebottle(), new device_coyote2(), new device_dgbutton() };

NimBLEScan *pBLEScan;
boolean scanthread_is_scanning;

int scanTime = 30;  // Duration is in seconds in NimBLE

NimBLEAdvertisedDevice *found_bledevice;
Device *found_device;

class PulsemoteAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *advertisedDevice) override {
    ESP_LOGI("comms-bt", "Advertised Device: %s", advertisedDevice->toString().c_str());
    // can't connect while scanning is going on - it locks up everything.
    found_device = nullptr;

    for (int i=0; i< ble_devices.size(); i++) {
      if (ble_devices[i]->is_device(advertisedDevice)) {
        found_device = ble_devices[i]->clone();
        break;
      }
    }   
    if (found_device) {
      found_bledevice = new NimBLEAdvertisedDevice(*advertisedDevice);
      NimBLEDevice::getScan()->stop();
    }
  }
};

bool ble_get_service(NimBLERemoteService*& service, NimBLEClient* bleClient, NimBLEUUID uuid) {
  ESP_LOGD("get_service", "Getting service %s", uuid.toString().c_str());
  service = bleClient->getService(uuid);
  if (service == nullptr) {
    ESP_LOGE("get_service", "Failed to find service UUID: %s", uuid.toString().c_str());
    return false;
  }
  return true;
}

bool ble_get_characteristic(NimBLERemoteService* service, NimBLERemoteCharacteristic*& c, NimBLEUUID uuid, notify_callback notifyCallback = nullptr) {
  ESP_LOGD("get_char", "Getting characteristic %s", uuid.toString().c_str());
  c = service->getCharacteristic(uuid);
  if (c == nullptr) {
    ESP_LOGE("get_char", "Failed to find characteristic UUID: %s", uuid.toString().c_str());
    return false;
  }
  if (!notifyCallback)
    return true;
  // we want notifications
  if (c->canNotify() && c->subscribe(true, notifyCallback))
    return true;
  else {
    ESP_LOGE("get_char", "Failed to register for notifications for characteristic UUID: %s", uuid.toString().c_str());
    return false;
  }
}

bool ble_get_characteristic_response(NimBLERemoteService* service, NimBLERemoteCharacteristic*& c, NimBLEUUID uuid, notify_callback notifyCallback = nullptr) {
  ESP_LOGD("dgb", "Getting characteristic %s", uuid.toString().c_str());
  c = service->getCharacteristic(uuid);
  if (c == nullptr) {
    ESP_LOGE("dgb", "Failed to find characteristic UUID: %s", uuid.toString().c_str());
    return false;
  }

  if (!notifyCallback)
    return true;

  // we want notifications
  if (c->canNotify() && c->subscribe(true, notifyCallback, true)) {
    ESP_LOGI("dgb","subscribed to notifications");
    return true;
  }
  else {
    ESP_LOGE("dgb", "Failed to register for notifications for characteristic UUID: %s", uuid.toString().c_str());
    return false;
  }
}

void scan_comms_init(void) {
  NimBLEDevice::init("x");
  NimBLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_ADV);  // send advertisements with 6 dbm
  pBLEScan = NimBLEDevice::getScan();         // create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new PulsemoteAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);  // active scan uses more power, but get results faster
  pBLEScan->setInterval(250);
  pBLEScan->setWindow(125);  // less or equal setInterval value
  ESP_LOGI("comms-bt", "Started ble scanning task");
}

void scan_loop() {
  boolean repeatscan = false;  // if we found something and connected to it, keep scanning for more

  do {
    ESP_LOGI("comms-bt", "Scanning for %ds", scanTime);
    pBLEScan->start(scanTime, false);  // up to one minute

    if (found_device && found_bledevice) {
      ESP_LOGI(found_device->getShortName(), "found device");
      vTaskDelay(pdMS_TO_TICKS(100));
      found_device->set_callback(device_change_handler);
      boolean connected = found_device->connect_to_device(found_bledevice);
      if (!connected) {
        ESP_LOGD(found_device->getShortName(),"connection failed");
        delete found_device;    
        found_device = NULL;
      }
      ESP_LOGD("comms-bt","removing bledevice and scan again");
      delete found_bledevice;
      repeatscan = true;
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    pBLEScan->clearResults();  // delete results fromBLEScan buffer to release memory
  } while (repeatscan);
}

// We scan in a separate task - scanning is a blocking
// operation. All the communication with the Bluetooth devices also happens
// in this task.

// Currently we scan forever, but we might want to stop after the first
// scan_loop() (which runs for some number of seconds after the last thing is connected)
// until something manual causes it to start again (like a disconnection or
// pushing a manual start scan button)

void TaskCommsBT(void *pvParameters) {
  scanthread_is_scanning = false;
  scan_comms_init();
  vTaskDelay(pdMS_TO_TICKS(2000)); // time for serial/debug to be ready
  while (true) {
    scanthread_is_scanning = true;
    scan_loop();
    scanthread_is_scanning = false;
    vTaskDelay(pdMS_TO_TICKS(1000));  // Scan for X seconds, wait for 1 second
  }
}
