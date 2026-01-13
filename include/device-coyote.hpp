#pragma once

#include <coyote.hpp>
#include <device.hpp>

bool temporary_has_a_coyote(void);

class device_coyote : public Device {
 public:
  bool is_device(const NimBLEAdvertisedDevice* advertisedDevice) {
    if (!coyote.is_coyote(advertisedDevice))
      return false;
    //if (temporary_has_a_coyote()) {
    //  return false;
    //}
    return true;
  }

  DeviceType getType() const override { return DeviceType::device_coyote; }

  Coyote& get() { return coyote; }

  int getmodel() { return coyote.getmodel(); }

  void set_ab_mode(int a, int b) {
    if (a>=0) coyote.chan_a().put_setmode((coyote_mode)a);
    if (b>=0) coyote.chan_b().put_setmode((coyote_mode)b);
  }

  const char* getShortName() const override { 
    int x = const_cast<device_coyote*>(this)->getmodel(); // careful
    
    if (x == 2) {
      return "Coyote2"; 
    } else {
      return "Coyote3";
    }
  }

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
    coyote.set_callback(std::bind(&device_coyote::coyote_change_handler, this, std::placeholders::_1));
  };

  bool connect_to_device(NimBLEAdvertisedDevice* device) override {
    return coyote.connect_to_device(device);
  };

  void coyote_change_handler(coyote_type_of_change t) {  // not enough states for a map
    type_of_change ct = D_NONE;
    if (t == C_NONE) ct = D_NONE;
    if (t == C_CONNECTING) ct = D_CONNECTING;
    if (t == C_DISCONNECTED) ct = D_DISCONNECTED;
    if (t == C_CONNECTED) ct = D_CONNECTED;
    // we also get other change events, but don't need to do anything special with them
    device_change_handler(ct, this);
  };

  Device* clone() const override {
    return new device_coyote();
  }

  private:
    Coyote coyote;
};