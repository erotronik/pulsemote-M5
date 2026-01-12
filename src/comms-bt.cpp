#include <NimBLEDevice.h>
#include <esp_log.h>

#include "device-coyote.hpp"
#include "device-mk312.hpp"
#include "device-thrustalot.hpp"
#include "device-lovense.hpp"
#include "device-funosr.hpp"
#include "device-ossm.hpp"
#include "device-bubblebottle.hpp"
#include "device-loop.hpp"
#include "device-dgbutton.hpp"
#include "device.hpp"
#include "comms-bt.hpp"

// An instance of each device is used for scanning
std::vector<Device*> ble_devices = { new device_loop(), new device_mk312(), new device_ossm(), new device_thrustalot(), new device_bubblebottle(), new device_coyote(), new device_dgbutton(), new device_lovense(), new device_funosr() };

NimBLEScan *pBLEScan;
NimBLEAdvertisedDevice *found_bledevice;
Device *found_device;
bool scanthread_is_scanning = false;

class PulsemoteAdvertisedDeviceCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override {
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

bool ble_get_characteristic(NimBLERemoteService* service, NimBLERemoteCharacteristic*& c, NimBLEUUID uuid, NimBLERemoteCharacteristic::notify_callback notifyCallback, bool response) {
  ESP_LOGD("get_char", "Getting characteristic %s", uuid.toString().c_str());
  c = service->getCharacteristic(uuid);
  if (c == nullptr) {
    ESP_LOGE("get_char", "Failed to find characteristic UUID: %s", uuid.toString().c_str());
    return false;
  }
  if (!notifyCallback) return true;
  // we want notifications
  if (c->canNotify() && c->subscribe(true, notifyCallback, response))
    return true;
  else {
    ESP_LOGE("get_char", "Failed to register for notifications for characteristic UUID: %s", uuid.toString().c_str());
    return false;
  }
}

void scan_comms_init(void) {
  NimBLEDevice::init("m5");
  //NimBLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_ADV);  // send advertisements with 6 dbm
  pBLEScan = NimBLEDevice::getScan(); // create new scan
  pBLEScan->setScanCallbacks(new PulsemoteAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // active scan uses more power, but get results faster
  pBLEScan->setInterval(512);
  pBLEScan->setWindow(64); // less or equal setInterval value
  ESP_LOGI("comms-bt", "Started ble scanning task");
}

void scan_loop() {
  for (;;) {
    bool repeatscan = false;  // reset each pass

    ESP_LOGI("comms-bt", "Scanning for %ds on core%d", scanTime, xPortGetCoreID());
    pBLEScan->getResults(scanTime * 1000, false);  // blocks until timeout or stop()
    ESP_LOGI("comms-bt", "Scanning stopped");

    // Take ownership of what the callback found, then clear globals
    auto* dev = found_device;
    auto* adv = found_bledevice;
    found_device = nullptr;
    found_bledevice = nullptr;

    if (dev && adv) {
      ESP_LOGI("comms-bt", "found device: %s", dev->getShortName());

      vTaskDelay(pdMS_TO_TICKS(100));
      dev->set_callback(device_change_handler);

      bool connected = dev->connect_to_device(adv);
      if (!connected) {
        // give NimBLE task time to finish any pending callbacks
        vTaskDelay(pdMS_TO_TICKS(250));
        ESP_LOGD("comms-bt", "%s connection failed", dev->getShortName());
        delete dev;
        dev = nullptr;
      }

      delete adv;
      adv = nullptr;

      // If we successfully connected (or you want to continue regardless), rescan
      repeatscan = (dev != nullptr);   // or just `true` if you always want to rescan
      // If you keep `dev`, make sure you have a plan to delete it later.
    } else {
      // If one is set and the other isn't, that's a race/logic bug worth logging
      if (dev || adv) {
        ESP_LOGW("comms-bt", "partial find (dev=%p adv=%p) - race?", dev, adv);
        delete dev;
        delete adv;
      }
    }

    pBLEScan->clearResults();
    vTaskDelay(pdMS_TO_TICKS(10));

    if (!repeatscan) break;
  }
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
    vTaskDelay(pdMS_TO_TICKS(2000));  // Scan for X seconds, wait for X second
  }
}
