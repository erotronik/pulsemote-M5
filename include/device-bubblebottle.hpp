#pragma once

#include <NimBLEDevice.h>
#include <Venerate.h>

#include <device.hpp>

class device_bubblebottle_NimBLEClientCallback;
class device_bubblebottle;

class device_bubblebottle : public Device {
 public:
  device_bubblebottle();
  ~device_bubblebottle();

  QueueHandle_t events;

  DeviceType getType() const override { return DeviceType::device_bubblebottle; }
  const char* getShortName() const override { return "Bubbler"; }

  bool connect_to_device(NimBLEAdvertisedDevice* device) override;
  void set_callback(device_callback c) override;

  bool is_device(NimBLEAdvertisedDevice* advertisedDevice) override;
  boolean get_isconnected();

  Device* clone() const override {
    return new device_bubblebottle();
  }

 private:
  static const int mkbuffer_maxlen = 100;
  int bottle_state;
  void ble_bubblebottle_send(String newValue);
  void ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                       uint8_t* pData, size_t length, bool isNotify);
  NimBLEClient* bleClient = nullptr;
  friend class DeviceBubblebottleNimBLEClientCallback;
  bool is_connected = false;
  device_callback update_callback;
  void connected_callback();
  void disconnected_callback(int reason);
  void notify(type_of_change);
  NimBLERemoteService* bubblebottleService;
  NimBLERemoteCharacteristic* uuid_rx_Characteristic;
  NimBLERemoteCharacteristic* uuid_tx_Characteristic;

};
