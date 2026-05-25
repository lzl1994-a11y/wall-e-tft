#pragma once

namespace WallE {

/**
 * 中文：日志输出接口，隐藏具体输出介质（当前实现是串口）。
 * English: Logging interface that hides the concrete output backend (Serial now).
 */
class ILogger {
 public:
  /**
   * 中文：虚析构函数，允许通过接口指针安全释放派生类。
   * English: Virtual destructor so derived logger objects can be deleted safely.
   */
  virtual ~ILogger() = default;

  /**
   * 中文：初始化日志系统；通常在 setup() 或控制器启动时调用一次。
   * English: Initializes the logger; normally called once from setup() or controller startup.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  virtual void begin() = 0;

  /**
   * 中文：输出普通信息级别日志。
   * English: Writes an informational log message.
   *
   * @param message 中文：以 '\0' 结尾的日志文本；允许由实现决定如何处理空指针。
   *                English: Null-terminated log text; null handling is implementation-defined.
   * @return 无 / None.
   */
  virtual void info(const char* message) = 0;

  /**
   * 中文：输出警告级别日志，用于可恢复或非致命问题。
   * English: Writes a warning log for recoverable or non-fatal issues.
   *
   * @param message 中文：以 '\0' 结尾的日志文本。
   *                English: Null-terminated log text.
   * @return 无 / None.
   */
  virtual void warn(const char* message) = 0;

  /**
   * 中文：输出错误级别日志，用于初始化失败等严重问题。
   * English: Writes an error log for serious problems such as initialization failure.
   *
   * @param message 中文：以 '\0' 结尾的日志文本。
   *                English: Null-terminated log text.
   * @return 无 / None.
   */
  virtual void error(const char* message) = 0;
};

}  // namespace WallE
