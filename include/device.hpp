#pragma once

#include <functional>
#include <memory>

enum type_of_change { D_NONE, D_DISCONNECTED, D_CONNECTING, D_CONNECTED };

enum class DeviceType { splashscreen, device_mk312, device_coyote2, device_thrustalot, device_bubblebottle
 };

class Device {
 public:
  virtual DeviceType getType() const = 0;
  virtual const char *getShortName() const = 0;
  virtual ~Device() {}
};