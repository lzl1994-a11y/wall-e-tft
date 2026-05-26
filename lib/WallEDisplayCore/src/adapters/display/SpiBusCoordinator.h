#pragma once

#include <Arduino.h>
#include <stddef.h>

namespace WallE {

/**
 * 中文：SPI 片选协调器，只负责把一组设备 CS 初始化为未选中并在切换设备前统一拉高。
 * English: SPI chip-select coordinator that initializes a set of device CS pins as deselected and raises all CS pins before switching devices.
 */
class SpiBusCoordinator {
 public:
  /**
   * 中文：一个 SPI 设备的片选描述。
   * English: Chip-select description for one SPI device.
   */
  struct Device {
    /**
     * 中文：构造设备片选描述，兼容较旧的 C++ 标准。
     * English: Constructs a device CS descriptor while staying compatible with older C++ standards.
     *
     * @param pin 中文：设备片选引脚；小于 0 表示未定义。
     *            English: Device CS pin; negative means undefined.
     * @param isEnabled 中文：是否启用该设备。
     *                  English: Whether the device is enabled.
     */
    Device(int pin = -1, bool isEnabled = true) : csPin(pin), enabled(isEnabled) {}

    /// 中文：设备片选引脚；小于 0 表示该设备没有片选或未接入。
    /// English: Device chip-select pin; negative means the device has no CS or is not connected.
    int csPin;

    /// 中文：该设备是否参与本轮协调。
    /// English: Whether this device participates in coordination.
    bool enabled;
  };

  /**
   * 中文：构造协调器；调用方必须保证 devices 数组生命周期长于协调器。
   * English: Constructs the coordinator; caller must keep the devices array alive longer than the coordinator.
   *
   * @param devices 中文：设备片选描述数组。
   *                English: Device CS descriptor array.
   * @param count 中文：数组元素数量。
   *              English: Number of descriptors.
   */
  SpiBusCoordinator(const Device* devices, size_t count) : devices_(devices), count_(count) {}

  /**
   * 中文：把所有启用设备的有效 CS 配置为 OUTPUT，并拉高到未选中状态。
   * English: Configures every enabled valid CS as OUTPUT and drives it HIGH as deselected.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void begin() const {
    for (size_t i = 0; i < count_; ++i) {
      const Device& device = devices_[i];
      if (device.enabled && device.csPin >= 0) {
        pinMode(device.csPin, OUTPUT);
        digitalWrite(device.csPin, HIGH);
      }
    }
  }

  /**
   * 中文：统一取消选择所有启用的 SPI 设备；在访问任意屏幕或字库前调用。
   * English: Deselects all enabled SPI devices; call before accessing any display or font flash.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void deselectAll() const {
    for (size_t i = 0; i < count_; ++i) {
      const Device& device = devices_[i];
      if (device.enabled && device.csPin >= 0) {
        digitalWrite(device.csPin, HIGH);
      }
    }
  }

 private:
  /// 中文：需要协调的设备片选描述数组；不由协调器释放。
  /// English: Device CS descriptor array to coordinate; not owned by the coordinator.
  const Device* devices_ = nullptr;

  /// 中文：设备片选描述数量。
  /// English: Number of device CS descriptors.
  size_t count_ = 0;
};

}  // namespace WallE
