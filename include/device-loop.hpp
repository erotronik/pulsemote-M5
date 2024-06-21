#pragma once

#include <NimBLEDevice.h>
#include <device.hpp>

class device_loop_NimBLEClientCallback;
class device_loop;

class device_loop : public Device {
 public:
  device_loop();
  ~device_loop();

  QueueHandle_t events;

  DeviceType getType() const override { return DeviceType::device_loop; }
  const char* getShortName() const override { return "Loop"; }
  bool connect_to_device(NimBLEAdvertisedDevice* device) override;
  void set_callback(device_callback c) override;
  bool is_device(NimBLEAdvertisedDevice* advertisedDevice) override;
  Device* clone() const override {
    return new device_loop();
  }

  boolean get_isconnected();
  int get_reading(void);

 private:
  void ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
  void connected_callback();
  void disconnected_callback(int reason);
  void notify(type_of_change);

  NimBLEClient* bleClient = nullptr;
  NimBLERemoteService* loopService;
  NimBLERemoteCharacteristic* uuid_rx_Characteristic;
  friend class DeviceloopNimBLEClientCallback;

  bool is_connected = false;
  device_callback update_callback;
  int loopreading;
  int nreadings;
};
