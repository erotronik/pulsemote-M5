#pragma once

#include <NimBLEDevice.h>
#include <device.hpp>

class device_funosr_NimBLEClientCallback;
class device_funosr;

class device_funosr : public Device {
 public:
  device_funosr();
  ~device_funosr();

  DeviceType getType() const override { return DeviceType::device_funosr; }
  const char* getShortName() const override { return "Stroker"; }
  
  bool connect_to_device(NimBLEAdvertisedDevice* device) override;
  void set_callback(device_callback c) override;

  bool is_device(NimBLEAdvertisedDevice* advertisedDevice) override;

  void funosr_stroke(int smin, int smax, int duration);
  void funosr_stop(void);
  void funosr_goto(int pos, int duration=0);

  Device* clone() const override {
    return new device_funosr();
  }

 private:

  void ble_funosr_send(String newValue);
  void ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
  NimBLEClient* bleClient = nullptr;
  friend class DevicefunosrNimBLEClientCallback;
  device_callback update_callback;
  void connected_callback();
  void disconnected_callback(int reason);
  void notify(type_of_change);
  NimBLERemoteService* funosrService;
  NimBLERemoteCharacteristic* uuid_rx_Characteristic;
  NimBLERemoteCharacteristic* uuid_tx_Characteristic;

};
