#pragma once

#include <NimBLEDevice.h>
#include <device.hpp>

class device_lovense_NimBLEClientCallback;
class device_lovense;

class device_lovense : public Device {
 public:
  device_lovense();
  ~device_lovense();

  const char* patterns[12] = {"Constant", "RampDown",  "FastBuzz",  "RampUp"};
  const int patterns_n = 4;

  DeviceType getType() const override { return DeviceType::device_lovense; }
  const char* getShortName() const override { return "Lovense"; }
  
  bool connect_to_device(NimBLEAdvertisedDevice* device) override;
  void set_callback(device_callback c) override;

  bool is_device(NimBLEAdvertisedDevice* advertisedDevice) override;

  void setmodespeed(int mode, int speed);

  Device* clone() const override {
    return new device_lovense();
  }
  int ble_lovense_getbattery(void);

 private:

  void ble_lovense_send(String newValue);

  void ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
  NimBLEClient* bleClient = nullptr;
  friend class DevicelovenseNimBLEClientCallback;
  device_callback update_callback;
  void connected_callback();
  void disconnected_callback(int reason);
  void notify(type_of_change);
  NimBLERemoteService* lovenseService;
  NimBLERemoteCharacteristic* uuid_rx_Characteristic;
  NimBLERemoteCharacteristic* uuid_tx_Characteristic;

  QueueHandle_t notifyQueue;

  static const int NOTIFY_QUEUE_LEN =5;
  static const int NOTIFY_MAX_DATA = 64;

  struct NotifyPacket {
    size_t length;
    uint8_t data[NOTIFY_MAX_DATA];
  };

};
