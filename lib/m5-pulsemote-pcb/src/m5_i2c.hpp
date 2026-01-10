#pragma once
#include <stdint.h>
#include <stddef.h>
#include <M5Unified.h>

class I2CBusM5 {
public:
  explicit I2CBusM5(m5::I2C_Class& b, uint32_t freq_hz = 400000)
    : bus(b), freq(freq_hz) {}

  // Write: START + addr(W) + reg + data... + STOP
  bool write_reg(uint8_t addr7, uint8_t reg, const uint8_t* data, size_t len) {
    if (!bus.start(addr7, false, freq)) return false;
    if (!bus.write(reg)) { bus.stop(); return false; }
    if (len && !bus.write(data, len)) { bus.stop(); return false; }
    return bus.stop();
  }

  // Read: START + addr(W) + reg + RESTART + addr(R) + read(len) + STOP
  bool read_reg(uint8_t addr7, uint8_t reg, uint8_t* data, size_t len) {
    if (!bus.start(addr7, false, freq)) return false;
    if (!bus.write(reg)) { bus.stop(); return false; }

    if (!bus.restart(addr7, true, freq)) { bus.stop(); return false; }
    if (len && !bus.read(data, len)) { bus.stop(); return false; }

    return bus.stop();
  }

private:
  m5::I2C_Class& bus;
  uint32_t freq;
};
