#include "adapters/pca9685/Pca9685Driver.h"
#include "config/Config.h"
#include <Arduino.h>
#include <Wire.h>

namespace WallE {

Pca9685Driver::Pca9685Driver(int sdaPin, int sclPin)
    : sdaPin_(sdaPin), sclPin_(sclPin) {
  for (size_t i = 0; i < kChannelCount; ++i) {
    lastValues_[i] = UINT16_MAX;
  }
}

void Pca9685Driver::begin() {
  Wire.begin(sdaPin_, sclPin_);
  Wire.setClock(WallEConfig::kPca9685I2cHz);
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

  const size_t channelCount =
      (count < kChannelCount) ? count : kChannelCount;
  uint16_t normalized[kChannelCount];
  for (size_t i = 0; i < channelCount; ++i) {
    const int32_t value = values[i];
    if (value <= 0) {
      normalized[i] = 0;
    } else if (value >= 65535) {
      normalized[i] = 4095;
    } else {
      normalized[i] = static_cast<uint16_t>(value >> 4);
    }
  }

  size_t channel = 0;
  while (channel < channelCount) {
    while (channel < channelCount &&
           normalized[channel] == lastValues_[channel]) {
      ++channel;
    }
    if (channel >= channelCount) {
      break;
    }

    const size_t firstChanged = channel;
    while (channel < channelCount &&
           normalized[channel] != lastValues_[channel]) {
      ++channel;
    }
    const size_t changedCount = channel - firstChanged;
    if (writeChannelRange(static_cast<uint8_t>(firstChanged),
                          &normalized[firstChanged], changedCount)) {
      for (size_t i = firstChanged; i < channel; ++i) {
        lastValues_[i] = normalized[i];
      }
    }
  }
}

bool Pca9685Driver::writeChannelRange(uint8_t firstChannel,
                                      const uint16_t* values,
                                      size_t count) {
  if (values == nullptr || count == 0 ||
      firstChannel + count > kChannelCount) {
    return false;
  }

  Wire.beginTransmission(kPca9685Addr);
  Wire.write(static_cast<uint8_t>(0x06 + 4 * firstChannel));
  for (size_t i = 0; i < count; ++i) {
    uint16_t on = 0;
    uint16_t off = values[i];
    if (values[i] == 0) {
      off = 4096;
    } else if (values[i] >= 4095) {
      on = 4096;
      off = 0;
    }
    Wire.write(static_cast<uint8_t>(on));
    Wire.write(static_cast<uint8_t>(on >> 8));
    Wire.write(static_cast<uint8_t>(off));
    Wire.write(static_cast<uint8_t>(off >> 8));
  }
  return Wire.endTransmission() == 0;
}

}  // namespace WallE
