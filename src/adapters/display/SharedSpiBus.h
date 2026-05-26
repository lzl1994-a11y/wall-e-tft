#pragma once

#include "adapters/display/SpiBusCoordinator.h"
#include "config/Config.h"

namespace WallE {

/**
 * 中文：返回本项目的共享 SPI 片选协调器；这里是项目配置到通用 SPI 边界的唯一转换点。
 * English: Returns the project shared-SPI chip-select coordinator; this is the only adapter from project config to the generic SPI boundary.
 *
 * @param 无 / None.
 * @return 中文：共享 SPI 片选协调器引用。
 *         English: Reference to the shared-SPI chip-select coordinator.
 */
inline const SpiBusCoordinator& sharedSpiBus() {
  static const SpiBusCoordinator::Device devices[] = {
      {WallEConfig::kTftCs, true},
      {WallEConfig::kScreenFontCs, WallEConfig::kUseScreenFontFlash},
      {WallEConfig::eysTftCs, WallEConfig::kEnableEyeDisplay},
  };
  static const SpiBusCoordinator bus(devices, sizeof(devices) / sizeof(devices[0]));
  return bus;
}

/**
 * 中文：初始化所有共享 SPI 设备的片选脚，避免多个设备同时被选中。
 * English: Initializes CS pins of all shared-SPI devices so multiple devices are not selected at once.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
inline void beginSharedSpiCsPins() {
  sharedSpiBus().begin();
}

/**
 * 中文：取消选择所有共享 SPI 设备；在切换屏幕或字库 Flash 前调用。
 * English: Deselects all shared-SPI devices; call before switching between display and font flash.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
inline void deselectSharedSpiDevices() {
  sharedSpiBus().deselectAll();
}

}  // namespace WallE
