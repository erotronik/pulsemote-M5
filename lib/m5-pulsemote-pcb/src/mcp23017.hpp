#pragma once
#include <stdint.h>
#include <M5Unified.h>
#include "m5_i2c.hpp"

// Minimal MCP23017 (I2C) helper using M5Unified m5::I2C_Class register helpers.
// - BANK=0 register map
// - Supports: begin, setupInterrupts (similar intent), pinMode (INPUT / INPUT_PULLUP),
//   setupInterruptPin (CHANGE), readGPIOA/B, getCapturedInterrupt (INTCAP), clearInterrupts.

class MCP23017 {
public:
  MCP23017() = default;

  void attach(const m5::I2C_Class& i2c, uint8_t addr7, uint32_t freq_hz = 400000) {
    bus = &i2c;
    addr = addr7;
    freq = freq_hz;
  }

  void setClock(uint32_t freq_hz) { freq = freq_hz; }

  bool begin() {
    uint8_t iocon = 0;
    if (!bus->readRegister(addr, IOCON, &iocon, 1, freq)) return false;
    // Force BANK=0 (bit7=0). Keep SEQOP=0 for sequential reads.
    iocon &= ~(1u << 7); // BANK = 0
    iocon &= ~(1u << 5); // SEQOP = 0
    if (!bus->writeRegister8(addr, IOCON, iocon, freq)) return false;
    if (!bus->writeRegister8(addr, IOCON2, iocon, freq)) return false;
    return true;
  }

  // setupInterrupts(mirror, openDrain, polarity)
  bool setupInterrupts(bool mirror, bool openDrain, uint8_t polarityHigh) {
    uint8_t iocon = 0;
    if (!bus->readRegister(addr, IOCON, &iocon, 1, freq)) return false;

    // BANK already forced 0 in begin(); keep it 0.
    iocon &= ~(1u << 7); // BANK=0
    iocon &= ~(1u << 5); // SEQOP=0

    if (mirror)       iocon |=  (1u << 6); else iocon &= ~(1u << 6); // MIRROR
    if (openDrain)    iocon |=  (1u << 2); else iocon &= ~(1u << 2); // ODR
    if (polarityHigh == HIGH) iocon |=  (1u << 1); else iocon &= ~(1u << 1); // INTPOL

    if (!bus->writeRegister8(addr, IOCON,  iocon, freq)) return false;
    if (!bus->writeRegister8(addr, IOCON2, iocon, freq)) return false;
    return true;
  }

  // Arduino-like modes
  //  - INPUT:        direction=input, pullup disabled
  //  - INPUT_PULLUP: direction=input, pullup enabled
  bool pinMode(uint8_t pin, uint8_t mode) {
    if (pin > 15) return false;

    uint16_t iodir = 0;
    if (!read16(IODIRA, iodir)) return false;
    iodir |= (1u << pin);                 // input
    if (!write16(IODIRA, iodir)) return false;

    uint16_t gppu = 0;
    if (!read16(GPPUA, gppu)) return false;

    if (mode == XINPUT_PULLUP) gppu |=  (1u << pin);
    else                      gppu &= ~(1u << pin);

    return write16(GPPUA, gppu);
  }

  // Equivalent to setupInterruptPin(pin, CHANGE)
  bool setupInterruptPin(uint8_t pin, uint8_t mode) {
    (void)mode; // we only implement CHANGE here
    if (pin > 15) return false;
    // INTCON bit = 0 => compare against previous value (CHANGE)
    uint16_t intcon = 0;
    if (!read16(INTCONA, intcon)) return false;
    intcon &= ~(1u << pin);
    if (!write16(INTCONA, intcon)) return false;

    // Enable interrupt-on-change
    uint16_t gpinten = 0;
    if (!read16(GPINTENA, gpinten)) return false;
    gpinten |= (1u << pin);
    return write16(GPINTENA, gpinten);
  }

  uint8_t readGPIOA() {
  uint8_t v = 0xFF;  // default to pulled-up (safe)
  bus->readRegister(addr, GPIOA, &v, 1, freq);
  return v;
}

uint8_t readGPIOB() {
  uint8_t v = 0xFF;
  bus->readRegister(addr, GPIOB, &v, 1, freq);
  return v;
}

uint16_t readGPIOAB() {
  uint8_t buf[2] = {0xFF, 0xFF};
  bus->readRegister(addr, GPIOA, buf, 2, freq); // GPIOA then GPIOB
  return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

  uint16_t getCapturedInterrupt() {
    uint8_t buf[2] = {0xFF, 0xFF};
    bus->readRegister(addr, INTCAPA, buf, 2, freq);
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
  }

  // Clear interrupts:
  // Reading INTCAP (or GPIO) clears the interrupt condition for the pins that triggered.
  // We'll read INTCAP and ignore the result.
  bool clearInterrupts() {
    uint8_t buf[2];
    return bus->readRegister(addr, INTCAPA, buf, 2, freq);
  }

  static constexpr uint8_t XINPUT        = 0x00;
  static constexpr uint8_t XINPUT_PULLUP = 0x02;

private:
  const m5::I2C_Class* bus = nullptr;
  uint8_t addr;
  uint32_t freq;

  // BANK=0 (paired) register map
  static constexpr uint8_t IODIRA   = 0x00;
  static constexpr uint8_t IODIRB   = 0x01;
  static constexpr uint8_t GPINTENA = 0x04;
  static constexpr uint8_t GPINTENB = 0x05;
  static constexpr uint8_t INTCONA  = 0x08;
  static constexpr uint8_t INTCONB  = 0x09;
  static constexpr uint8_t IOCON    = 0x0A; // also at 0x0B
  static constexpr uint8_t IOCON2   = 0x0B;
  static constexpr uint8_t GPPUA    = 0x0C;
  static constexpr uint8_t GPPUB    = 0x0D;
  static constexpr uint8_t INTCAPA  = 0x10;
  static constexpr uint8_t INTCAPB  = 0x11;
  static constexpr uint8_t GPIOA    = 0x12;
  static constexpr uint8_t GPIOB    = 0x13;

  bool read16(uint8_t regA, uint16_t& out) {
    uint8_t buf[2] = {0, 0};
    if (!bus->readRegister(addr, regA, buf, 2, freq)) return false;
    out = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    return true;
  }

  bool write16(uint8_t regA, uint16_t v) {
    uint8_t buf[2] = {(uint8_t)(v & 0xFF), (uint8_t)(v >> 8)};
    return bus->writeRegister(addr, regA, buf, 2, freq);
  }
};
