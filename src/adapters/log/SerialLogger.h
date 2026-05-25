#pragma once

#include "config/Config.h"
#include "ports/ILogger.h"
#include <Arduino.h>

namespace WallE {

/**
 * 中文：基于 Arduino Serial 的日志实现。
 * English: Logger implementation based on Arduino Serial.
 */
class SerialLogger : public ILogger {
 public:
  /**
   * 中文：启动串口并等待短暂时间，保证 USB 串口枚举后可以看到日志。
   * English: Starts Serial and waits briefly so logs can be visible after USB Serial enumeration.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void begin() override {
    Serial.begin(WallEConfig::kSerialBaud);
    delay(300);
  }

  /**
   * 中文：输出 INFO 级别日志。
   * English: Outputs an INFO-level log.
   *
   * @param message 中文：日志文本。
   *                English: Log text.
   * @return 无 / None.
   */
  void info(const char* message) override { log("INFO", message); }

  /**
   * 中文：输出 WARN 级别日志。
   * English: Outputs a WARN-level log.
   *
   * @param message 中文：日志文本。
   *                English: Log text.
   * @return 无 / None.
   */
  void warn(const char* message) override { log("WARN", message); }

  /**
   * 中文：输出 ERROR 级别日志。
   * English: Outputs an ERROR-level log.
   *
   * @param message 中文：日志文本。
   *                English: Log text.
   * @return 无 / None.
   */
  void error(const char* message) override { log("ERROR", message); }

 private:
  /**
   * 中文：按统一格式 [LEVEL] message 写入串口。
   * English: Writes a Serial line in the unified format [LEVEL] message.
   *
   * @param level 中文：日志级别文本，例如 INFO。
   *              English: Log level text such as INFO.
   * @param message 中文：日志正文。
   *                English: Log body.
   * @return 无 / None.
   */
  void log(const char* level, const char* message) {
    Serial.print('[');
    Serial.print(level);
    Serial.print("] ");
    Serial.println(message);
  }
};

}  // namespace WallE
