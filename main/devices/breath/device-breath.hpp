#pragma once

#include <NimBLEDevice.h>
#include <device.hpp>

class device_breath_NimBLEClientCallback;
class device_breath;

class device_breath : public Device {
 public:
  device_breath();
  ~device_breath();

  DeviceType getType() const override { return DeviceType::device_breath; }
  const char* getShortName() const override { return "Breath"; }
  
  bool connect_to_device(NimBLEAdvertisedDevice* device) override;
  void set_callback(device_callback c) override;

  bool is_device(const NimBLEAdvertisedDevice* advertisedDevice) override;

  void breath_block(int pc);

  Device* clone() const override {
    return new device_breath();
  }

 private:

  void ble_breath_send(const char* newValue);
  void ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
  NimBLEClient* bleClient = nullptr;
  friend class DevicebreathNimBLEClientCallback;
  device_callback update_callback;
  void connected_callback();
  void disconnected_callback(int reason);
  void notify(type_of_change);
  NimBLERemoteService* breathService;
  NimBLERemoteCharacteristic* uuid_rx_Characteristic;
  NimBLERemoteCharacteristic* uuid_tx_Characteristic;
};
