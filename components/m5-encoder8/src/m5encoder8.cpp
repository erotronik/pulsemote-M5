#include "m5encoder8.hpp"

#ifdef CONFIG_HASM5ENCODER

M5Encoder8::~M5Encoder8() {
}

void M5Encoder8::init() {
}

void M5Encoder8::update() {
}

bool M5Encoder8::begin() {
    auto ad = readRegister8(ENCODER8_I2C_ADDRESS_REG);
    ESP_LOGI("m5encoder8", "Read address %x %x", ad.value_or(-1),_dev->getAddress());   
 
    return ( ad.has_value() && ad.value_or(0) != -1);
}

bool M5Encoder8::writeRegister(uint8_t reg, uint8_t *buffer, uint8_t length) {
    ESP_LOGE("writeregister","reg=%x len=%d",reg,length);
    return _dev->writeRegister(reg, buffer, length);
}

bool M5Encoder8::readRegister(uint8_t reg, uint8_t *buffer, uint8_t length) {
    ESP_LOGE("readregister","reg=%x",reg);
    return _dev->readRegister(reg, buffer, length);
}

std::optional<uint8_t> M5Encoder8::readRegister8(uint8_t reg) {
    ESP_LOGE("readregister8","reg=%x",reg);
    return _dev->readRegister8(reg);
}

int32_t M5Encoder8::getEncoderValue(uint8_t index) {
    uint8_t data[4];
    uint8_t reg = index * 4 + ENCODER8_ENCODER_REG;
    readRegister(reg, data, 4);
    int32_t value =
        data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    return value;
}

void M5Encoder8::setEncoderValue(uint8_t index, int32_t value) {
    uint8_t data[4];
    uint8_t reg = index * 4 + ENCODER8_ENCODER_REG;

    data[0] = value & 0xff;
    data[1] = (value >> 8) & 0xff;
    data[2] = (value >> 16) & 0xff;
    data[3] = (value >> 24) & 0xff;
    writeRegister(reg, data, 4);
}

int32_t M5Encoder8::getIncrementValue(uint8_t index) {
    uint8_t data[4];
    uint8_t reg = index * 4 + ENCODER8_INCREMENT_REG;
    readRegister(reg, data, 4);
    int32_t value =
        data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    return value;
}

bool M5Encoder8::getButtonStatus(uint8_t index) {
    uint8_t data;
    uint8_t reg = index + ENCODER8_BUTTON_REG;
    readRegister(reg, &data, 1);
    return data;
}

bool M5Encoder8::getSwitchStatus(void) {
    uint8_t data;
    readRegister(ENCODER8_SWITCH_REG, &data, 1);
    return data;
}

void M5Encoder8::setLEDColor(uint8_t index, uint32_t color) {
    uint8_t data[4];
    data[2]     = color & 0xff;
    data[1]     = (color >> 8) & 0xff;
    data[0]     = (color >> 16) & 0xff;
    uint8_t reg = index * 3 + ENCODER8_RGB_LED_REG;
    writeRegister(reg, data, 3);
}

void M5Encoder8::setAllLEDColor(uint32_t color) {
    uint8_t data[27];
    for (int i = 0; i < 9; i++) {
        int base = i * 3;
        data[base]     = color & 0xff;
        data[base + 1] = (color >> 8) & 0xff;
        data[base + 2] = (color >> 16) & 0xff;
    }
    writeRegister(ENCODER8_RGB_LED_REG, data, 27);
}

void M5Encoder8::resetCounter(uint8_t index) {
    uint8_t data[4];
    data[0] = 1;
    uint8_t reg = index + ENCODER8_RESET_COUNTER_REG;
    writeRegister(reg, data, 1);
}

uint8_t M5Encoder8::getFirmwareVersion(void) {
  return readRegister8(ENCODER8_FIRMWARE_VERSION_REG).value_or(0);
}

#endif