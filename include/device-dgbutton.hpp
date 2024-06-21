#pragma once

#include <NimBLEDevice.h>
#include <Venerate.h>

#include <device.hpp>

class device_dgbutton_NimBLEClientCallback;
class device_dgbutton;

class device_dgbutton : public Device {
 public:
  device_dgbutton();
  ~device_dgbutton();

  QueueHandle_t events;

  DeviceType getType() const override { return DeviceType::device_dgbutton; }
  const char* getShortName() const override { return "DG Button"; }

  bool connect_to_device(NimBLEAdvertisedDevice* device) override;
  void set_callback(device_callback c) override;

  bool is_device(NimBLEAdvertisedDevice* advertisedDevice) override;

  Device* clone() const override {
    return new device_dgbutton();
  }

  int get_battery(void) { return battery; };

  enum class dgbutton_event { NONE, PUSH, RELEASE, HELD};

 private:

  int battery = 0;

  enum class dgbutton_state { WFHELLO, SENDSTART, LISTEN};
  dgbutton_state state = dgbutton_state::WFHELLO;

  void ble_dgbutton_send(String newValue);
  void ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
  NimBLEClient* bleClient = nullptr;
  friend class DevicedgbuttonNimBLEClientCallback;
  device_callback update_callback;
  void connected_callback();
  void disconnected_callback(int reason);
  void notify(type_of_change);
  NimBLERemoteService* dgbuttonService;
  NimBLERemoteCharacteristic* rx_Characteristic;
  NimBLERemoteCharacteristic* tx_Characteristic;
};
