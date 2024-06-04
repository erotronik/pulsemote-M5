#pragma once

#include <NimBLEDevice.h>
#include <Venerate.h>
#include <device.hpp>

class device_mk312_NimBLEClientCallback;
class device_mk312;

class device_mk312 : public Device {
 public:
  device_mk312();
  ~device_mk312();

  DeviceType getType() const override { return DeviceType::device_mk312; }
  const char* getShortName() const override { return "MK-312B"; }

  bool connect_to_device(NimBLEAdvertisedDevice* device_mk312_device) override;
  void set_callback(device_callback c) override;

  const char* etmodes[22] = {"Waves",  "Stroke", "Climb",  "Combo",  "Intense",
                             "Rhythm", "Audio1", "Audio2", "Audio3", "Split",
                             "Rand1",  "Rand2",  "Toggle", "Orgasm", "Tormnt",
                             "Phase1", "Phase2", "Phase3", "User1",  "User2",
                             "User3",  "User4"};

  boolean get_isconnected();
  bool is_device(NimBLEAdvertisedDevice* advertisedDevice) override;
  void set_mode(int p);
  void etbox_on(byte mode);
  void etbox_off(void);
  int get_mode(void);
  void etbox_setbyte(word a, byte d);
  byte etbox_getbyte(word a);

  Device* clone() const override {
    return new device_mk312(*this);
  }

 private:
  bool getService(NimBLERemoteService*& service, NimBLEUUID uuid);
  void ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                       uint8_t* pData, size_t length, bool isNotify);
  NimBLEClient* bleClient = nullptr;
  friend class DeviceMK312NimBLEClientCallback;
  bool is_connected = false;
  device_callback update_callback;
  void connected_callback();
  void disconnected_callback(int reason);
  void notify(type_of_change);
  NimBLERemoteService* mk312Service;
  NimBLERemoteCharacteristic* uuid_rxtx_Characteristic;
  Venerate BOX = Venerate(0);

  void etbox_flushcb(void);
  void etbox_txcb(byte c);
  int etbox_rxcb(char* p, int x);

  static const int mkbuffer_maxlen = 100;
  static const int mktx_maxlen = 20;  // For sending via bluetooth max is 20 bytes

  byte mkbuffer[mkbuffer_maxlen];
  byte mkwptr = 0;
  byte mkrptr = 0;
  byte mktx[mktx_maxlen];
  byte mktx_n = 0;
};
