#pragma once

namespace WallE {

/**
 * 中文：应用主状态机的状态。
 * English: States used by the main application state machine.
 */
enum class AppState {
  /// 中文：正在启动或初始化硬件。
  /// English: The application is booting or initializing hardware.
  Booting,

  /// 中文：系统已准备好接收串口命令。
  /// English: The system is ready to receive serial commands.
  Ready,

};

}  // namespace WallE
