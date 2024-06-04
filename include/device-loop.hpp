#pragma once

#include <NimBLEDevice.h>
#include <device.hpp>

class device_loop_NimBLEClientCallback;
class device_loop;

typedef std::function<void(type_of_change change, Device* d)> device_callback;

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
  boolean get_isconnected();
  int get_reading(void);

  Device* clone() const override {
    return new device_loop(*this);
  }

 private:
  int bottle_state;
  void ble_loop_send(String newValue);
  bool getService(NimBLERemoteService*& service, NimBLEUUID uuid);
  void ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                       uint8_t* pData, size_t length, bool isNotify);
  NimBLEClient* bleClient = nullptr;
  friend class DeviceloopNimBLEClientCallback;
  bool is_connected = false;
  device_callback update_callback;
  void connected_callback();
  void disconnected_callback(int reason);
  void notify(type_of_change);
  NimBLERemoteService* loopService;
  NimBLERemoteCharacteristic* uuid_rx_Characteristic;
  int loopreading;
  int loopreading_max;
  int loopreading_min;
  int nreadings;

};
