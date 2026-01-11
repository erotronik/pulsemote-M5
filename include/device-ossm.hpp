#pragma once

#include <NimBLEDevice.h>
#include <device.hpp>
#include <ArduinoJson.h>

class device_ossm_NimBLEClientCallback;
class device_ossm;

class device_ossm : public Device {
 public:
  device_ossm();
  ~device_ossm();

  const char* patterns[12] = {"Stroke"};
  const int patterns_n = 1;

  DeviceType getType() const override { return DeviceType::device_ossm; }
  const char* getShortName() const override { return "OSSM"; }
  
  bool connect_to_device(NimBLEAdvertisedDevice* device) override;
  void set_callback(device_callback c) override;
  
  void set_speed(int s);
  void set_stroke(int s);
  void set_depth(int s);
  void set_sensation(int s);
  void set_pattern(int s);

  bool got_data_yet();

  int get_speed(void);
  int get_stroke(void);
  int get_depth(void);
  int get_sensation(void);
  int get_pattern(void);

  bool is_device(const NimBLEAdvertisedDevice* advertisedDevice) override;

  const char* pattern_name_for_idx(int idx);

  Device* clone() const override {
    return new device_ossm();
  }

 private:

  void ble_ossm_send(const char*newValue);
  char send_buf[32];
  JsonDocument rxstatus;
  JsonDocument patternlist;
  void ble_mk_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
  NimBLEClient* bleClient = nullptr;
  friend class DeviceossmNimBLEClientCallback;
  device_callback update_callback;
  void connected_callback();
  void disconnected_callback(int reason);
  void notify(type_of_change);
  NimBLERemoteService* ossmService;
  NimBLERemoteCharacteristic* ossm_rx_Characteristic;
  NimBLERemoteCharacteristic* ossm_tx_Characteristic;
  NimBLERemoteCharacteristic* ossm_speedknob_Characteristic;
  NimBLERemoteCharacteristic* ossm_patternlist_Characteristic;
};
