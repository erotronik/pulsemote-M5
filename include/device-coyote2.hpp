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

  Coyote& get() {  return coyote; }

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

  void set_callback(device_callback c) override {
    coyote.set_callback(std::bind(&device_coyote2::change_handler, this, std::placeholders::_1));
  };

  bool connect_to_device(NimBLEAdvertisedDevice* device) override {
    return coyote.connect_to_device(device);
  };

  void change_handler(coyote_type_of_change t) {  // not enough states for a map
    type_of_change ct = D_NONE;
    if (t == C_NONE) ct = D_NONE;
    if (t == C_CONNECTING) ct = D_CONNECTING;
    if (t == C_DISCONNECTED) ct = D_DISCONNECTED;
    if (t == C_CONNECTED) ct = D_CONNECTED;
    // we also get other change events, but don't need to do anything special with them
    device_change_handler(ct, this);
  };

  Device* clone() const override {
    return new device_coyote2();
  }

  private:
    Coyote coyote;
};