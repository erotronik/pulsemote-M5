#pragma once

#include <NimBLEDevice.h>
#include <Venerate.h>
#include <device.hpp>
#include <freertos/stream_buffer.h>

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

  bool is_device(const NimBLEAdvertisedDevice* advertisedDevice) override;
  void set_mode(int p);
  void etbox_on(int mode);
  void etbox_off(void);
  int get_last_mode(void);
  void etbox_setbyte(uint16_t a, uint8_t d);
  void etbox_setlevela(uint8_t d);
  void etbox_setlevelb(uint8_t d);
  void etbox_setpanellock(bool x);
  uint8_t etbox_getbyte(uint16_t a);
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

  StreamBufferHandle_t notifyStream = nullptr;
  static constexpr size_t NOTIFY_STREAM_SIZE = 128;

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
  void etbox_txcb(uint8_t c);
  int etbox_rxcb(char* p, int x);

  static const int mktx_maxlen = 20;  // For sending via bluetooth max is 20 bytes
  uint8_t mktx[mktx_maxlen];
  uint8_t mktx_n = 0;
};
