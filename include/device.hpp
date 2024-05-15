#pragma once

#include <functional>
#include <memory>
// #include <NimBLEDevice.h>

enum class DeviceType { splashscreen, device_mk312, device_coyote2, device_mqtt };

class Device {
 public:
  virtual DeviceType getType() const = 0;
  virtual const char *getShortName() const = 0;
  // virtual bool is_our_device(NimBLEAdvertisedDevice* advertisedDevice) = 0;
  virtual ~Device() {}  // Virtual destructor
};