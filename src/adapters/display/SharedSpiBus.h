#pragma once

#include "config/Config.h"
#include <Arduino.h>

namespace WallE {

/**
 * 中文：如果引脚有效（>=0），把该引脚拉高。
 * English: Drives a pin HIGH when the pin number is valid (>= 0).
 *
 * @param pin 中文：待操作的 GPIO 引脚；小于 0 表示未定义。
 *            English: GPIO pin to drive; negative means undefined.
 * @return 无 / None.
 */
inline void setPinHighIfDefined(int pin) {
  if (pin >= 0) {
    digitalWrite(pin, HIGH);
  }
}

/**
 * 中文：如果片选引脚有效，则配置为 OUTPUT 并默认拉高为未选中状态。
 * English: Configures a valid chip-select pin as OUTPUT and drives it HIGH by default.
 *
 * @param pin 中文：片选 GPIO；小于 0 表示该设备没有片选。
 *            English: Chip-select GPIO; negative means this device has no CS pin.
 * @return 无 / None.
 */
inline void beginCsPinIfDefined(int pin) {
  if (pin >= 0) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
}

/**
 * 中文：初始化所有共享 SPI 设备的片选脚，避免多个设备同时被选中。
 * English: Initializes CS pins of all shared-SPI devices so multiple devices are not selected at once.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
inline void beginSharedSpiCsPins() {
  beginCsPinIfDefined(WallEConfig::kTftCs);
  beginCsPinIfDefined(WallEConfig::kFontCs);
  beginCsPinIfDefined(WallEConfig::eysTftCs);
}

/**
 * 中文：取消选择所有共享 SPI 设备；在切换屏幕或字库 Flash 前调用。
 * English: Deselects all shared-SPI devices; call before switching between display and font flash.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
inline void deselectSharedSpiDevices() {
  setPinHighIfDefined(WallEConfig::kTftCs);
  setPinHighIfDefined(WallEConfig::kFontCs);
  setPinHighIfDefined(WallEConfig::eysTftCs);
}

}  // namespace WallE
