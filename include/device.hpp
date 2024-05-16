#pragma once

#include <functional>
#include <memory>

enum class DeviceType { splashscreen, device_mk312, device_coyote2, device_wifi };

class Device {
 public:
  virtual DeviceType getType() const = 0;
  virtual const char *getShortName() const = 0;
  virtual ~Device() {}
};