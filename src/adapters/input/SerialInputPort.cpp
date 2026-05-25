#include "adapters/input/SerialInputPort.h"

namespace WallE {

/**
 * 中文：初始化串口输入缓存状态；串口本身已由 SerialLogger::begin() 启动。
 * English: Initializes Serial input buffer state; Serial itself is already started by SerialLogger::begin().
 *
 * @param 无 / None.
 * @return 无 / None.
 */
void SerialInputPort::begin() {
  length_ = 0;
  readyLength_ = 0;
  lastRxMs_ = millis();
  Serial.println("serial input ready: one CR/LF-delimited line is one message");
}

/**
 * 中文：从 Serial 读取字节并尝试组成一条完整输入包。
 * English: Reads bytes from Serial and tries to assemble one complete input packet.
 *
 * @param[out] out 中文：当返回 true 时，写入完整输入包的数据指针和长度。
 *                  English: When true is returned, receives the complete packet pointer and length.
 * @return 中文：true 表示本次取到一条完整消息；false 表示还没有完整消息。
 *         English: true when one complete message is available; false otherwise.
 */
bool SerialInputPort::poll(InputPacket& out) {
  if (readyLength_ > 0) {
    // 中文：优先返回上一次已经提交但尚未被取走的数据。
    // English: Return previously committed data first if it has not been consumed yet.
    out.data = ready_;
    out.length = readyLength_;
    readyLength_ = 0;
    return true;
  }

  while (Serial.available() > 0) {
    // 中文：ch 使用 int 是为了保留 Serial.read() 的 -1 错误值。
    // English: ch is int to preserve the -1 error value returned by Serial.read().
    int ch = Serial.read();
    if (ch < 0) {
      break;
    }
    if (ch == '\r' || ch == '\n') {
      // 中文：CR/LF 是强制消息边界；空行不会产生消息。
      // English: CR/LF is a forced message boundary; empty lines do not produce packets.
      if (commitIfNeeded(true)) {
        out.data = ready_;
        out.length = readyLength_;
        readyLength_ = 0;
        return true;
      }
      continue;
    }
    if (length_ < sizeof(buffer_)) {
      // 中文：只保存不超过 kInputMaxBytes 的内容，避免写越界。
      // English: Keep only up to kInputMaxBytes bytes to avoid buffer overflow.
      buffer_[length_++] = static_cast<uint8_t>(ch);
    }
    lastRxMs_ = millis();
  }

  // 中文：如果没有换行，但串口已经空闲一段时间，也把当前缓存提交成一条消息。
  // English: If no newline arrives but Serial has been idle long enough, commit the buffer as one message.
  if (commitIfNeeded(false)) {
    out.data = ready_;
    out.length = readyLength_;
    readyLength_ = 0;
    return true;
  }
  return false;
}

/**
 * 中文：丢弃所有尚未处理的串口输入，通常在系统未 Ready 时调用。
 * English: Drops all pending Serial input, usually called when the system is not Ready.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
void SerialInputPort::drain() {
  while (Serial.available() > 0) {
    Serial.read();
  }
  length_ = 0;
  readyLength_ = 0;
}

/**
 * 中文：把临时接收缓存复制到 ready_ 缓存，供 poll() 返回。
 * English: Copies the temporary receive buffer into ready_ so poll() can return it.
 *
 * @param force 中文：true 表示立即提交；false 表示只有超过空闲时间才提交。
 *              English: true commits immediately; false commits only after the idle timeout.
 * @return 中文：true 表示已经提交；false 表示当前还不该提交。
 *         English: true when committed; false when it should not be committed yet.
 */
bool SerialInputPort::commitIfNeeded(bool force) {
  if (length_ == 0) {
    return false;
  }
  if (!force && millis() - lastRxMs_ < WallEConfig::kSerialIdleMs) {
    return false;
  }

  readyLength_ = length_;
  memcpy(ready_, buffer_, readyLength_);
  length_ = 0;
  return true;
}

}  // namespace WallE
