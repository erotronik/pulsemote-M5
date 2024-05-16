#include <NimBLEDevice.h>
#include <esp_log.h>

#include <memory>

#include "device-coyote2.hpp"
#include "device-mk312.hpp"
#include "device-thrustalot.hpp"
#include "device.hpp"

device_coyote2 *coyote_device_controller;
device_mk312 *device_mk312_controller;
device_thrustalot *device_thrustalot_controller;

void device_change_handler(type_of_change t, Device *d);

void coyote_change_handler(coyote_type_of_change t) {
  // hack for now
  return device_change_handler(static_cast<type_of_change>(t),
                               static_cast<Device *>(coyote_device_controller));
}

NimBLEServer *pServer = nullptr;
NimBLECharacteristic *pTxCharacteristic;
NimBLEScan *pBLEScan;
boolean scanthread_is_scanning = false;
boolean scanthread_found_something = false;

int scanTime = 60;  // Duration is in seconds in NimBLE

NimBLEAdvertisedDevice *coyote_ble_device = nullptr;
NimBLEAdvertisedDevice *device_mk312_device = nullptr;
NimBLEAdvertisedDevice *device_thrustalot_device = nullptr;

class PulsemoteAdvertisedDeviceCallbacks
    : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *advertisedDevice) {
    ESP_LOGI("comms-bt", "Advertised Device: %s\n",
             advertisedDevice->toString().c_str());
    if (coyote_device_controller->is_device(advertisedDevice)) {
      ESP_LOGI(coyote_device_controller->getShortName(), "found device");
      // can't connect while scanning is going on - it locks up everything.
      scanthread_found_something = true;
      coyote_ble_device = new NimBLEAdvertisedDevice(*advertisedDevice);
      NimBLEDevice::getScan()->stop();
    }
    if (device_mk312_controller->is_device(advertisedDevice)) {
      ESP_LOGI(device_mk312_controller->getShortName(), "found device");
      scanthread_found_something = true;
      device_mk312_device = new NimBLEAdvertisedDevice(*advertisedDevice);
      NimBLEDevice::getScan()->stop();
    }
    if (device_thrustalot_controller->is_device(advertisedDevice)) {
      ESP_LOGI(device_thrustalot_controller->getShortName(), "found device");
      scanthread_found_something = true;
      device_thrustalot_device = new NimBLEAdvertisedDevice(*advertisedDevice);
      NimBLEDevice::getScan()->stop();
    }

  }
};

void scan_comms_init(void) {
  NimBLEDevice::init("x");
  NimBLEDevice::setPower(
      ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_ADV);  // send advertisements with 6 dbm
  pBLEScan = NimBLEDevice::getScan();         // create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(
      new PulsemoteAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(
      true);  // active scan uses more power, but get results faster
  pBLEScan->setInterval(250);
  pBLEScan->setWindow(125);  // less or equal setInterval value
  ESP_LOGI("comms-bt", "Started ble scanning task\n");
}

void scan_loop() {
  boolean repeatscan = true;  // if we found something and connected to it, keep
                              // scanning for more
  scanthread_is_scanning = true;

  while (repeatscan) {
    ESP_LOGI("comms-nt", "Scanning for %ds\n", scanTime);
    pBLEScan->start(scanTime, false);  // up to one minute
    repeatscan = false;
    pBLEScan->clearResults();  // delete results fromBLEScan buffer to release
                               // memory

    if (scanthread_found_something) {
      scanthread_found_something = false;
      if (coyote_ble_device) {
        coyote_device_controller->get().connect_to_device(coyote_ble_device);
        delete coyote_ble_device;
        coyote_ble_device = nullptr;
      } else if (device_mk312_device) {
        boolean connected =
            device_mk312_controller->connect_to_device(device_mk312_device);
        delete device_mk312_device;
        device_mk312_device = nullptr;
        if (!connected) {
          delete device_mk312_controller;
          device_mk312_controller = new device_mk312();
          device_mk312_controller->set_callback(device_change_handler);
        }
      } else if (device_thrustalot_device) {
        boolean connected =
            device_thrustalot_controller->connect_to_device(device_thrustalot_device);
        delete device_thrustalot_device;
        device_thrustalot_device = nullptr;
      }
      repeatscan = true;
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
  scanthread_is_scanning = false;
}

// On the ESP32, we scan in a separate task - scanning is a blocking
// operation. All the communication with the Coyote also happens
// in this task.

// Currently we scan forever, but we might want to stop after the first
// scan_loop() (which runs for 60 seconds after the last thing is connected)
// until something manual causes it to start again (like a disconnection or
// pushing a manual start scan button)

void TaskCommsBT(void *pvParameters) {
  coyote_device_controller = new device_coyote2();
  device_mk312_controller = new device_mk312();
  device_thrustalot_controller = new device_thrustalot();

  coyote_device_controller->get().set_callback(coyote_change_handler);
  device_mk312_controller->set_callback(device_change_handler);
  device_thrustalot_controller->set_callback(device_change_handler);

  scan_comms_init();
  vTaskDelay(pdMS_TO_TICKS(5000)); // time for serial/debug to be ready
  while (true) {
    scan_loop();
    vTaskDelay(pdMS_TO_TICKS(1000));  // Scan for 60 seconds, wait for 1 second
  }
}
