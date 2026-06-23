#include "adapters/pca9685/Pca9685Driver.h"
#include <Arduino.h>
#include <Wire.h>

namespace WallE {

Pca9685Driver::Pca9685Driver(int sdaPin, int sclPin)
    : sdaPin_(sdaPin), sclPin_(sclPin) {}

void Pca9685Driver::begin() {
  Wire.begin(sdaPin_, sclPin_);
  reset();
  freq(50.0);
}

void Pca9685Driver::write8(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kPca9685Addr);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

uint8_t Pca9685Driver::read8(uint8_t reg) {
  Wire.beginTransmission(kPca9685Addr);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)kPca9685Addr, (uint8_t)1);
  return Wire.read();
}

void Pca9685Driver::reset() {
  write8(0x00, 0x00);
}

void Pca9685Driver::freq(float f) {
  uint8_t prescale = (uint8_t)(25000000.0 / 4096.0 / f + 0.5);
  uint8_t old_mode = read8(0x00);
  write8(0x00, (old_mode & 0x7F) | 0x10); // Mode 1, sleep
  write8(0xFE, prescale);                 // Prescale
  write8(0x00, old_mode);                 // Mode 1
  delayMicroseconds(5);                   // sleep_us(5)
  write8(0x00, old_mode | 0xA1);          // Mode 1, autoincrement on
}

void Pca9685Driver::pwm(uint8_t index, uint16_t on, uint16_t off) {
  Wire.beginTransmission(kPca9685Addr);
  Wire.write(0x06 + 4 * index);
  Wire.write(on & 0xFF);
  Wire.write(on >> 8);
  Wire.write(off & 0xFF);
  Wire.write(off >> 8);
  Wire.endTransmission();
}

void Pca9685Driver::duty(uint8_t index, uint16_t value, bool invert) {
  if (value > 4095) {
    value = 4095;
  }
  if (invert) {
    value = 4095 - value;
  }
  if (value == 0) {
    pwm(index, 0, 4096);
  } else if (value == 4095) {
    pwm(index, 4096, 0);
  } else {
    pwm(index, 0, value);
  }
}

void Pca9685Driver::setChannels(const int32_t* values, size_t count) {
  if (count == 0 || values == nullptr) {
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    int32_t val = values[i];
    // Host sends large value (e.g. 65535 for 100%, 4915 for 1.5ms). PCA9685 is 12-bit.
    uint16_t pcaVal = (val <= 0) ? 0 : (uint16_t)(val >> 4); 
    duty(i, pcaVal);
  }
}

}  // namespace WallE
