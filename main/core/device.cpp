#include "device.hpp"

Device::~Device() {}

void Device::set_callback(device_callback c) {}

bool Device::is_device(const NimBLEAdvertisedDevice* advertisedDevice) {
    return false;
}

bool Device::connect_to_device(NimBLEAdvertisedDevice* device) {
    return false;
}

void Device::change_handler(type_of_change t) {
    device_change_handler(t, this);
}

bool Device::get_isconnected() {
    return is_connected;
}
