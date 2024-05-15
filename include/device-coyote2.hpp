#pragma once

#include <coyote.hpp>
#include <device.hpp>

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
  }

 private:
  Coyote coyote;
};