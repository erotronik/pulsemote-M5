#include <NimBLEDevice.h>
#include <esp_log.h>

#include "device-coyote.hpp"
#include "device-mk312.hpp"
#include "device-zc95.hpp"
#include "device-thrustalot.hpp"
#include "device-lovense.hpp"
#include "device-funosr.hpp"
#include "device-breath.hpp"
#include "device-ossm.hpp"
#include "device-bubblebottle.hpp"
#include "device-loop.hpp"
#include "device-dgbutton.hpp"
#include "device.hpp"
#include "comms-bt.hpp"
#include "lvgl-utils.h"

// An instance of each device is used for scanning
std::vector<Device*> ble_devices = { new device_loop(), new device_mk312(), new device_zc95(), new device_ossm(), new device_thrustalot(), new device_bubblebottle(), new device_coyote(), new device_dgbutton(), 
  new device_lovense(), new device_funosr(), new device_breath() };

NimBLEScan *pBLEScan;
NimBLEAdvertisedDevice *found_bledevice;
Device *found_device;
static volatile bool scanthread_is_scanning = false;
// Wakes the scan task for a new session. (Not a task notification: NimBLE uses those to wait)
static SemaphoreHandle_t scan_request = nullptr;
static portMUX_TYPE scan_mux = portMUX_INITIALIZER_UNLOCKED;
static unsigned long scan_until = 0;  // millis() when the scan session ends

static void extend_scan(void) {
  taskENTER_CRITICAL(&scan_mux);
  scan_until = millis() + scan_session_ms;
  taskEXIT_CRITICAL(&scan_mux);
}

// ms left in the scan session, 0 if it's over
static uint32_t scan_time_left(void) {
  taskENTER_CRITICAL(&scan_mux);
  long left = (long)(scan_until - millis());
  taskEXIT_CRITICAL(&scan_mux);
  return left > 0 ? left : 0;
}

void ble_start_scan(void) {
  extend_scan();
  if (scan_request) xSemaphoreGive(scan_request);
}

bool ble_is_scanning(void) {
  return scanthread_is_scanning;
}

class PulsemoteAdvertisedDeviceCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override {
    ESP_LOGI("comms-bt", "Advertised Device: %s", advertisedDevice->toString().c_str());
    // can't connect while scanning is going on - it locks up everything.
    // Once something's been found the scan is stopping, but more results can arrive meanwhile:
    // ignore them, rather than lose (and leak) what was found
    if (found_device) return;

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

// Scan until the session runs out, connecting whatever turns up. Each connection gives the session its
// full time again, so devices switched on one after another all get found

void scan_loop() {
  printf_log("Scanning for devices\n");
  for (;;) {
    uint32_t left = scan_time_left();
    if (left == 0) break;

    ESP_LOGI("comms-bt", "Scanning for %lus on core%d", (unsigned long)(left / 1000), xPortGetCoreID());
    pBLEScan->getResults(left, false);  // blocks until timeout or stop()
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
      printf_log("Connecting %s\n", dev->getShortName());  // there's no tab until it's connected

      bool connected = dev->connect_to_device(adv);
      if (!connected) {
        // give NimBLE task time to finish any pending callbacks
        vTaskDelay(pdMS_TO_TICKS(250));
        ESP_LOGD("comms-bt", "%s connection failed", dev->getShortName());
        delete dev;
        dev = nullptr;
      } else {
        extend_scan();
      }

      delete adv;
      adv = nullptr;
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
  }
  printf_log("Scan finished\n");
}

// We scan in a separate task - scanning is a blocking
// operation. All the communication with the Bluetooth devices also happens
// in this task. It waits for ble_start_scan() between scan sessions

void TaskCommsBT(void *pvParameters) {
  scanthread_is_scanning = false;
  scan_comms_init();
  vTaskDelay(pdMS_TO_TICKS(2000)); // time for serial/debug to be ready
  scan_request = xSemaphoreCreateBinary();
  extend_scan();  // scan at power-on
  while (true) {
    if (scan_time_left() > 0) {
      scanthread_is_scanning = true;
      scan_loop();
      scanthread_is_scanning = false;
    }
    xSemaphoreTake(scan_request, portMAX_DELAY);
  }
}
