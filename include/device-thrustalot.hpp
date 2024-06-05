#pragma once

#include <NimBLEDevice.h>
#include <Venerate.h>

#include <device.hpp>

class device_thrustalot_NimBLEClientCallback;
class device_thrustalot;

class device_thrustalot : public Device {
 public:
  device_thrustalot();
  ~device_thrustalot();

  DeviceType getType() const override { return DeviceType::device_thrustalot; }
  const char* getShortName() const override { return "Thrustalot"; }
  
  bool connect_to_device(NimBLEAdvertisedDevice* device) override;
  void set_callback(device_callback c) override;

  boolean get_isconnected();
  bool is_device(NimBLEAdvertisedDevice* advertisedDevice) override;

  void thrustallthewayin();
  void thrustallthewayout();
  void thrustonetime(int speed);
  void thrustsendspeed(int speed);
  const char *getpostext(void);

  QueueHandle_t events;

  Device* clone() const override {
    return new device_thrustalot();
  }

 private:
  int thrustcb_pos;
  int thrustcb_count;
  int thrustcb_left;

  void ble_thrustalot_send(String newValue);
  bool getService(NimBLERemoteService*& service, NimBLEUUID uuid);
  void ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
  NimBLEClient* bleClient = nullptr;
  friend class DeviceThrustalotNimBLEClientCallback;
  bool is_connected = false;
  device_callback update_callback;
  void connected_callback();
  void disconnected_callback(int reason);
  void notify(type_of_change);
  NimBLERemoteService* thrustService;
  NimBLERemoteCharacteristic* uuid_rx_Characteristic;
  NimBLERemoteCharacteristic* uuid_tx_Characteristic;

  static const int mkbuffer_maxlen =100;
  byte mkbuffer[mkbuffer_maxlen];
  byte mkwptr = 0;

};
