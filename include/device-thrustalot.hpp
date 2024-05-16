#pragma once

#include <NimBLEDevice.h>
#include <Venerate.h>

#include <device.hpp>

class device_thrustalot_NimBLEClientCallback;
class device_thrustalot;

#define mkbuffer_maxlen 100

typedef std::function<void(type_of_change change, Device* d)> device_callback;

// !B11X -- thrust to next half
// !B21XX -- old random
// !B51X -- speed +=21
// !B61X -- speed -=21
// !S000 -- speed 000 (to 255)  returns (blah)T:(\d+)\n(blah) for current thrust count

class device_thrustalot : public Device {
 public:
  device_thrustalot();
  ~device_thrustalot();

  boolean get_isconnected();
  static bool is_device(NimBLEAdvertisedDevice* advertisedDevice);
  bool connect_to_device(NimBLEAdvertisedDevice* device);
  void set_callback(device_callback c);

  DeviceType getType() const override { return DeviceType::device_thrustalot; }
  const char* getShortName() const override { return "Thrustalot"; }

  void thrustallthewayin();
  void thrustallthewayout();
  void thrustonetime(int speed);
  int thrustcb_pos;
  int thrustcb_count;
  int thrustcb_left;

  const char *getpostext(void) {
    if (thrustcb_pos ==1) return "In";
    if (thrustcb_pos ==2) return "Out";
    if (thrustcb_pos ==0) return "...";
    return "";
  }

 private:
  void ble_thrustalot_send(String newValue);
  bool getService(NimBLERemoteService*& service, NimBLEUUID uuid);
  void ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                       uint8_t* pData, size_t length, bool isNotify);
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

  byte mkbuffer[mkbuffer_maxlen];
  byte mkwptr = 0;

};
