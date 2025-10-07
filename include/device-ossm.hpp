#pragma once

#include <NimBLEDevice.h>
#include <device.hpp>

class device_ossm_NimBLEClientCallback;
class device_ossm;

class device_ossm : public Device {
 public:
  device_ossm();
  ~device_ossm();

  const char* patterns[12] = {"Constant"};
  const int patterns_n = 1;

  DeviceType getType() const override { return DeviceType::device_ossm; }
  const char* getShortName() const override { return "OSSM"; }
  
  bool connect_to_device(NimBLEAdvertisedDevice* device) override;
  void set_callback(device_callback c) override;
  void set_speed(int speed);

  bool is_device(NimBLEAdvertisedDevice* advertisedDevice) override;

  Device* clone() const override {
    return new device_ossm();
  }

 private:

  void ble_ossm_send(String newValue);

  void ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
  NimBLEClient* bleClient = nullptr;
  friend class DeviceossmNimBLEClientCallback;
  device_callback update_callback;
  void connected_callback();
  void disconnected_callback(int reason);
  void notify(type_of_change);
  NimBLERemoteService* ossmService;
  NimBLERemoteCharacteristic* ossm_rx_Characteristic;
  NimBLERemoteCharacteristic* ossm_tx_Characteristic;
  NimBLERemoteCharacteristic* ossm_speedknob_Characteristic;

};
