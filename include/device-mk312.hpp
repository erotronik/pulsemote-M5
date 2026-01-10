#pragma once

#include <NimBLEDevice.h>
#include <Venerate.h>
#include <device.hpp>

// temporary so we can get rid of Arduino.h everywhere
#ifndef byte
#define byte uint8_t
#endif

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

  static const int etmodes_n = 23;
  static const int etmodes_potluck = 22;
  const char* etmodes[etmodes_n] = {"Waves",  "Stroke", "Climb",  "Combo",  "Intense",
                             "Rhythm", "Audio1", "Audio2", "Audio3", "Split",
                             "Random1",  "Random2",  "Toggle", "Orgasm", "Torment",
                             "Phase1", "Phase2", "Phase3", "User1",  "User2",
                             "User3",  "User4", "Luck"};

  bool is_device(NimBLEAdvertisedDevice* advertisedDevice) override;
  void set_mode(int p);
  void etbox_on(int mode);
  void etbox_off(void);
  int get_last_mode(void);
  void etbox_setbyte(word a, byte d);
  void etbox_setlevela(byte d);
  void etbox_setlevelb(byte d);
  void etbox_setpanellock(bool x);
  byte etbox_getbyte(word a);
  void next_mode(void);
  int get_mode(void);
  bool connected(void);

  Device* clone() const override {
    return new device_mk312();
  }

 private:

  static const int potluck_n = 12;
  const uint8_t potluck[potluck_n] = {
    ETMODE_waves,
    ETMODE_stroke,
    ETMODE_climb, ETMODE_climb,
    ETMODE_rhythm,
    ETMODE_toggle,
    ETMODE_orgasm, ETMODE_orgasm,
    ETMODE_phase2, ETMODE_phase2, ETMODE_phase2, ETMODE_phase2
  };

  void ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                       uint8_t* pData, size_t length, bool isNotify);
  NimBLEClient* bleClient = nullptr;
  friend class DeviceMK312NimBLEClientCallback;
  device_callback update_callback;
  void connected_callback();
  void disconnected_callback(int reason);
  void notify(type_of_change);
  NimBLERemoteService* mk312Service;
  NimBLERemoteCharacteristic* uuid_rxtx_Characteristic;
  Venerate BOX = Venerate(0);
  int lastvalidmode =0;

  void etbox_flushcb(void);
  void etbox_txcb(byte c);
  int etbox_rxcb(char* p, int x);

  static const int mktx_maxlen = 20;  // For sending via bluetooth max is 20 bytes
  byte mktx[mktx_maxlen];
  byte mktx_n = 0;

  static const int NOTIFY_QUEUE_LEN =5; 
  static const int NOTIFY_MAX_DATA = 64;

  QueueHandle_t notifyQueue;

  struct NotifyPacket {
    size_t length;
    uint8_t data[NOTIFY_MAX_DATA];
  };

};
