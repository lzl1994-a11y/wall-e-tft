#pragma once

#include <stdint.h>
#include <stddef.h>

namespace WallE {

/**
 * 中文：PCA9685 驱动抽象接口，让应用逻辑不依赖底层 I2C 实现。
 * English: PCA9685 driver abstraction so application logic is independent of I2C implementation.
 */
class IPca9685Port {
 public:
  /**
   * 中文：虚析构函数，保证通过接口销毁派生类时行为正确。
   * English: Virtual destructor for safe destruction through the interface.
   */
  virtual ~IPca9685Port() = default;

  /**
   * 中文：初始化硬件，包括总线初始化和设备唤醒、分频设置。
   * English: Initializes the hardware including bus initialization, device wakeup, and prescaler.
   */
  virtual void begin() = 0;

  /**
   * 中文：向 PCA9685 连续写入多个通道的 PWM 值。
   * English: Continuously writes PWM values to multiple PCA9685 channels.
   *
   * @param values 中文：通道目标值（最高可传65535）。
   *               English: Target values (up to 65535).
   * @param count 中文：要写入的通道数量。
   *              English: Number of channels to write.
   */
  virtual void setChannels(const int32_t* values, size_t count) = 0;
};

}  // namespace WallE
