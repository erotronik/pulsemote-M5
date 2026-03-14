#pragma once

#include <M5Unified.h>

class M5Encoder8 {
public:
    ~M5Encoder8();

    static constexpr uint8_t ENCODER8_ENCODER_ADDR = 0x41;  
    static constexpr uint8_t ENCODER8_ENCODER_REG = 0x00;
    static constexpr uint8_t ENCODER8_INCREMENT_REG = 0x20;
    static constexpr uint8_t ENCODER8_BUTTON_REG = 0x50;
    static constexpr uint8_t ENCODER8_SWITCH_REG = 0x60;
    static constexpr uint8_t ENCODER8_RGB_LED_REG = 0x70;
    static constexpr uint8_t ENCODER8_RESET_COUNTER_REG = 0x40;
    static constexpr uint8_t ENCODER8_FIRMWARE_VERSION_REG = 0xFE;
    static constexpr uint8_t ENCODER8_I2C_ADDRESS_REG = 0xFF;

    M5Encoder8(std::uint8_t i2c_addr = ENCODER8_ENCODER_ADDR, std::uint32_t freq = 100000L,
		m5::I2C_Class* i2c = &m5::Ex_I2C)
      : _dev(new m5::I2C_Device(i2c_addr, freq, i2c)) {
          i2c->begin();
          ESP_LOGE("m5encoder8", "SDA=%d SCL=%d", i2c->getSDA(), i2c->getSCL());
      }

    void init();
    void update();
    bool begin();
    int32_t getEncoderValue(uint8_t index);
    void setEncoderValue(uint8_t index, int32_t value);
    int32_t getIncrementValue(uint8_t index);
    bool getButtonStatus(uint8_t index);
    bool getSwitchStatus(void);
    void setLEDColor(uint8_t index, uint32_t color);
    void setAllLEDColor(uint32_t color);
    uint8_t getFirmwareVersion(void);
    void resetCounter(uint8_t index);

    
private:   
    m5::I2C_Device* _dev;
    bool writeRegister(uint8_t reg, uint8_t *buffer, uint8_t length);
    bool readRegister(uint8_t reg, uint8_t *buffer, uint8_t length);
    std::optional<uint8_t> readRegister8(uint8_t reg);

};
