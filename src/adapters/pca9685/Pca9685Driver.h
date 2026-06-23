#pragma once

#include "ports/IPca9685Port.h"
#include <stdint.h>

namespace WallE {

/**
 * 中文：PCA9685 I2C 硬件驱动适配器。
 * English: PCA9685 I2C hardware driver adapter.
 */
class Pca9685Driver : public IPca9685Port {
 public:
  /**
   * 中文：初始化引脚（默认1和2）或由 Config 指定，并在类内维护。
   * English: Initializes with pins from Config, or default 1 and 2.
   *
   * @param sdaPin 中文：I2C SDA 引脚。 English: I2C SDA pin.
   * @param sclPin 中文：I2C SCL 引脚。 English: I2C SCL pin.
   */
  Pca9685Driver(int sdaPin, int sclPin);

  void begin() override;
  void setChannels(const int16_t* values, size_t count) override;

  // 严格参考 MicroPython 实现的基础底层方法
  void reset();
  void freq(float freq);
  void pwm(uint8_t index, uint16_t on, uint16_t off);
  void duty(uint8_t index, uint16_t value, bool invert = false);

 private:
  void write8(uint8_t reg, uint8_t value);
  uint8_t read8(uint8_t reg);

  int sdaPin_;
  int sclPin_;
  static constexpr uint8_t kPca9685Addr = 0x40;
};

}  // namespace WallE
