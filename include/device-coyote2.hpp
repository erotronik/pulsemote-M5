#pragma once

#include <coyote.hpp>
#include <device.hpp>

void device_change_handler(type_of_change t, Device *d);
typedef std::function<void(type_of_change change, Device* d)> device_callback;

class device_coyote2 : public Device {
 public:
  bool is_device(NimBLEAdvertisedDevice* advertisedDevice) {
    return coyote.is_coyote(advertisedDevice);
  }
  DeviceType getType() const override { return DeviceType::device_coyote2; }
  const char* getShortName() const override { return "Coyote"; }
  Coyote& get() { return coyote; }
  coyote_mode modes[3] = {M_BREATH, M_WAVES, M_NONE};
  const char *getModeName(int mode) {
    switch (mode) {
      case M_BREATH:
        return "Breath";
      case M_WAVES:
        return "Waves";
    }
    return "Off";
  };
  void set_callback(device_callback c) {
    coyote.set_callback(std::bind(&device_coyote2::change_handler, this, std::placeholders::_1));
  };
  bool connect_to_device(NimBLEAdvertisedDevice* device) {
    return coyote.connect_to_device(device);
  };
  void change_handler(coyote_type_of_change t) {
    device_change_handler(static_cast<type_of_change>(t), this);
  };

  private:
    Coyote coyote;
};