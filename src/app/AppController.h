#pragma once

#include "domain/AppState.h"
#include "domain/ChatSession.h"
#include "ports/IEyeDisplayPort.h"
#include "ports/IDisplayPort.h"
#include "ports/IInputPort.h"
#include "ports/ILogger.h"
#include "ports/IPca9685Port.h"

namespace WallE {

/**
 * 中文：应用总控制器，负责启动硬件、读取串口、分发主屏消息和眼睛动作。
 * English: Main application controller that starts hardware, reads serial data, and dispatches chat messages and eye actions.
 */
class AppController {
 public:
  /**
   * 中文：构造控制器并注入各个端口实现。
   * English: Constructs the controller and injects concrete port implementations.
   *
   * @param logger 中文：日志端口引用。
   *               English: Logger port reference.
   * @param input 中文：输入端口引用，当前为串口输入。
   *              English: Input port reference, currently Serial input.
   * @param display 中文：主屏显示端口引用，当前为 ST7789。
   *                English: Main display port reference, currently ST7789.
   * @param eyeDisplay 中文：眼睛显示端口引用，当前为 GC9A01。
   *                   English: Eye display port reference, currently GC9A01.
   * @param pca9685 中文：PCA9685 驱动端口引用。
   *                English: PCA9685 driver port reference.
   */
  AppController(ILogger& logger, IInputPort& input, IDisplayPort& display,
                IEyeDisplayPort& eyeDisplay, IPca9685Port& pca9685);

  /**
   * 中文：初始化日志、主屏、输入端口和初始会话状态。
   * English: Initializes logger, main display, input port, and initial session state.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void begin();

  /**
   * 中文：主循环入口；每次调用轮询串口并处理一条完整命令。
   * English: Main loop entry; each call polls Serial and handles one complete command if available.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void loop();

 private:
  enum class ScreenMode {
    Power,
    Chat,
  };

  /**
   * 中文：更新内部状态并同步主屏状态栏。
   * English: Updates the internal state and synchronizes the main display status bar.
   *
   * @param state 中文：新的应用状态。
   *              English: New application state.
   * @return 无 / None.
   */
  void setState(AppState state);
  void setChatOpen(bool open);
  void returnToPowerIfChatIdle();

  /// 中文：日志输出端口，不拥有对象生命周期。
  /// English: Logger output port; lifetime is owned elsewhere.
  ILogger& logger_;

  /// 中文：串口输入端口，不拥有对象生命周期。
  /// English: Serial input port; lifetime is owned elsewhere.
  IInputPort& input_;

  /// 中文：主屏显示端口，不拥有对象生命周期。
  /// English: Main display port; lifetime is owned elsewhere.
  IDisplayPort& display_;

  /// 中文：眼睛显示端口，不拥有对象生命周期。
  /// English: Eye display port; lifetime is owned elsewhere.
  IEyeDisplayPort& eyeDisplay_;

  /// 中文：PCA9685 驱动端口，不拥有对象生命周期。
  /// English: PCA9685 driver port; lifetime is owned elsewhere.
  IPca9685Port& pca9685_;

  /// 中文：主屏当前会话缓存。
  /// English: Current chat session cache for the main display.
  ChatSession session_;

  /// 中文：应用当前状态，默认从 Booting 开始。
  /// English: Current application state, starting from Booting by default.
  AppState state_ = AppState::Booting;
  ScreenMode screenMode_ = ScreenMode::Power;
  uint8_t powerPercent_ = 0;
  uint32_t lastChatActivityMs_ = 0;
  
  bool pca9685Rx_ = false;
  uint32_t bootMs_ = 0;
};

}  // namespace WallE
