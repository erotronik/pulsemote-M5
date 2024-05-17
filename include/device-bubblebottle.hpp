#pragma once

#include <NimBLEDevice.h>
#include <Venerate.h>

#include <device.hpp>

class device_bubblebottle_NimBLEClientCallback;
class device_bubblebottle;

#define mkbuffer_maxlen 100

typedef std::function<void(type_of_change change, Device* d)> device_callback;

class device_bubblebottle : public Device {
 public:
  device_bubblebottle();
  ~device_bubblebottle();

  boolean get_isconnected();
  static bool is_device(NimBLEAdvertisedDevice* advertisedDevice);
  bool connect_to_device(NimBLEAdvertisedDevice* device);
  void set_callback(device_callback c);

  void ble_bubblebottle_send(String newValue);
  DeviceType getType() const override { return DeviceType::device_bubblebottle; }
  const char* getShortName() const override { return "Bubbler"; }

  int bottle_state;
  bool bottle_changed_state = false;

 private:
  bool getService(NimBLERemoteService*& service, NimBLEUUID uuid);
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
