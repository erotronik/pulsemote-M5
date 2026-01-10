#pragma once
#include <stdint.h>
#include <M5Unified.h>
#include "m5_i2c.hpp"

class PCA9685 {
public:

  PCA9685() = default;

  void attach(const m5::I2C_Class& i2c, uint8_t addr7, uint32_t freq_hz = 400000) {
    bus = &i2c;
    addr = addr7;
    freq = freq_hz;
  }

  void setClock(uint32_t freq_hz) { freq = freq_hz; }

  // Equivalent to MODE1_AUTOINCR (+optional ALLCALL) and MODE2_INVERT
  bool begin(bool invert_outputs) {
    ESP_LOGE("PCA9685", "PCA9685 Begin bus=%p addr=0x%02x", bus, addr);
    // MODE1: AI=1 (auto-increment)
    uint8_t mode1 = (1u << 5);
    if (!bus->writeRegister8(addr, MODE1, mode1, freq)) return false;

    // MODE2: OUTDRV=1, optional INVRT
    uint8_t mode2 = (1u << 2);
    if (invert_outputs) mode2 |= (1u << 4);
    if (!bus->writeRegister8(addr, MODE2, mode2, freq)) return false;

    return true;
  }

  // Set PWM duty 0..4095
  bool setPWM(uint8_t channel, uint16_t value) {
    if (channel > 15) return false;
    value &= 0x0FFF;

    uint8_t reg = (uint8_t)(LED0_ON_L + 4 * channel);
    uint8_t data[4] = {
      0x00, 0x00,                         // ON = 0
      (uint8_t)(value & 0xFF),            // OFF_L
      (uint8_t)((value >> 8) & 0x0F)      // OFF_H
    };

    return bus->writeRegister(addr, reg, data, sizeof(data), freq);
  }

private:
  const m5::I2C_Class* bus = nullptr;
  uint8_t addr;
  uint32_t freq;

  static constexpr uint8_t MODE1 = 0x00;
  static constexpr uint8_t MODE2 = 0x01;
  static constexpr uint8_t LED0_ON_L = 0x06;
};
