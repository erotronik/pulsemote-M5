#pragma once

#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <device.hpp>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <string>
#include <vector>

class device_zc95 : public Device {
 public:
  device_zc95();
  ~device_zc95();

  DeviceType getType() const override { return DeviceType::device_zc95; }
  const char* getShortName() const override { return "ZC95"; }

  bool is_device(const NimBLEAdvertisedDevice* advertisedDevice) override;
  bool connect_to_device(NimBLEAdvertisedDevice* device) override;
  void set_callback(device_callback c) override;

  Device* clone() const override {
    return new device_zc95();
  }

  // Patterns the box can run remotely, read from it on connection
  int pattern_count(void) { return pattern_names.size(); }
  const char** get_pattern_names(void) { return pattern_names.data(); }
  const char* get_pattern_name(int index);

  bool pattern_start(int index);  // index into the pattern list, not the ZC95's pattern id
  bool pattern_stop(void);
  int get_running_pattern(void) { return running_pattern; }  // index, or -1 if stopped

  // Power for each channel, 0-1000. The box's front panel knobs are an upper limit
  bool set_power(int chan1, int chan2, int chan3, int chan4);

  bool connected(void);

  // Call regularly: deals with messages the box sends by itself
  void poll(void);

  // The limit set by the box's front panel knob for a channel (0-3), 0-1000. set_power() above this
  // has no further effect. 1000 until the box has reported it.
  int get_power_limit(int channel) { return power_limit[channel]; }

 private:
  bool get_version(void);
  bool get_patterns(void);
  bool request(JsonDocument& msg, JsonDocument* reply = nullptr);
  void send(const std::string& msg);

  void ble_zc95_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
  bool next_message(JsonDocument& doc, TickType_t wait);

  friend class DeviceZC95NimBLEClientCallback;
  void connected_callback();
  void disconnected_callback(int reason);
  void notify(type_of_change);
  void disconnect_and_delete(void);

  NimBLEClient* bleClient = nullptr;
  NimBLERemoteService* zc95Service = nullptr;
  NimBLERemoteCharacteristic* rxCharacteristic = nullptr;  // we write to this
  device_callback update_callback;

  // Messages are framed STX json ETX. Received data is put back together here
  std::string rx_buffer;
  bool rx_in_message = false;
  static constexpr size_t MAX_MESSAGE_SIZE = 4096;

  // Complete messages (std::string*) from NimBLE's task. One request at a time reads them,
  // waiting for the reply with its MsgId
  QueueHandle_t message_queue = nullptr;
  static constexpr int MESSAGE_QUEUE_LENGTH = 16;
  SemaphoreHandle_t request_mutex = nullptr;
  int last_msg_id = 0;

  struct pattern_t {
    int id;
    std::string name;
  };
  std::vector<pattern_t> patterns;
  std::vector<const char*> pattern_names;
  int running_pattern = -1;

  int power_limit[4] = {1000, 1000, 1000, 1000};
};
