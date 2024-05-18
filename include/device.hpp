#pragma once

#include <functional>
#include <memory>

class Device;
enum type_of_change { D_NONE, D_DISCONNECTED, D_CONNECTING, D_CONNECTED };
void device_change_handler(type_of_change t, Device *d);

enum class DeviceType { splashscreen, device_mk312, device_coyote2, device_thrustalot, device_bubblebottle
 };

class Device {
 public:
  virtual DeviceType getType() const = 0;
  virtual const char *getShortName() const = 0;
  virtual ~Device() {}
  virtual void change_handler(type_of_change t) {
    device_change_handler(t, this);
  };
};