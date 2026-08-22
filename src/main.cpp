#include "adapters/display/Gc9a01EyeDisplay.h"
#include "adapters/display/St7789DisplayPort.h"
#include "adapters/input/SerialInputPort.h"
#include "adapters/log/SerialLogger.h"
#include "adapters/network/WifiImageStreamClient.h"
#include "adapters/pca9685/Pca9685Driver.h"
#include "app/AppController.h"
#include "config/Config.h"

using namespace WallE;

/// 中文：串口日志实现，用于输出启动、警告和错误信息。
/// English: Serial logger implementation for startup, warning, and error messages.
SerialLogger logger;

/// 中文：串口输入实现，把一行串口内容解析为一个输入包。
/// English: Serial input implementation that turns one line of Serial data into one packet.
SerialInputPort input;

/// 中文：胸前主屏 ST7789 显示实现，用于聊天会话文本。
/// English: Chest main ST7789 display implementation for chat text.
St7789DisplayPort display;

/// 中文：眼睛 GC9A01 显示实现，用于播放眼睛动画。
/// English: Eye GC9A01 display implementation for eye animations.
Gc9a01EyeDisplay eyeDisplay;

/// 中文：PCA9685 舵机驱动实现，用于底层 I2C 通信。
/// English: PCA9685 driver implementation for I2C communication.
Pca9685Driver pca9685(WallEConfig::kPca9685Sda, WallEConfig::kPca9685Scl);

WifiImageStreamClient::Config makeImageStreamConfig() {
  WifiImageStreamClient::Config config;
  config.defaultNetworkConfig.setDefaults(
      WallEConfig::kWifi1Ssid, WallEConfig::kWifi1Password,
      WallEConfig::kWifi2Ssid, WallEConfig::kWifi2Password,
      WallEConfig::kWifi3Ssid, WallEConfig::kWifi3Password,
      WallEConfig::kImageServerHost, WallEConfig::kImageServerPort);
  config.maxJpegBytes = WallEConfig::kCameraMaxJpegBytes;
  config.wifiReconnectMs = WallEConfig::kWifiReconnectMs;
  config.tcpReconnectMs = WallEConfig::kTcpReconnectMs;
  config.pingMs = WallEConfig::kTcpPingMs;
  config.eventQueueDepth = WallEConfig::kImageEventQueueDepth;
  config.taskStackBytes = WallEConfig::kImageNetworkTaskStackBytes;
  config.taskPriority = WallEConfig::kImageNetworkTaskPriority;
  config.taskCore = WallEConfig::kImageNetworkTaskCore;
  return config;
}

WifiImageStreamClient imageStream(makeImageStreamConfig());

/// 中文：应用总控制器，连接输入、日志、主屏、眼睛屏和PCA9685。
/// English: Main application controller that wires input, logger, main display, eye display, and pca9685.
AppController controller(logger, input, display, eyeDisplay, pca9685,
                         imageStream);

/**
 * 中文：Arduino 启动入口，初始化可选眼睛屏和应用控制器。
 * English: Arduino startup entry; initializes the optional eye display and the application controller.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
void setup() {
  bool eyeDisplayReady = true;
  if (WallEConfig::kEnableEyeDisplay) {
    eyeDisplayReady = eyeDisplay.begin();
  }
  controller.begin();
  if (WallEConfig::kEnableEyeDisplay) {
    if (!eyeDisplayReady) {
      logger.error("eye display initialization failed");
    } else if (eyeDisplay.asyncDmaReady()) {
      logger.info("eye display async DMA ready");
    } else {
      logger.warn("eye display using synchronous fallback");
    }
  }
}

/**
 * 中文：Arduino 主循环入口，持续轮询串口并分发命令。
 * English: Arduino main loop entry; continuously polls Serial and dispatches commands.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
void loop() {
  controller.loop();
}
