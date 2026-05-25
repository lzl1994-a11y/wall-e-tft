#pragma once

#include "config/Config.h"
#include "ports/IInputPort.h"
#include <Arduino.h>

namespace WallE {

/**
 * 中文：串口输入适配器；把 CR/LF 分隔的一行数据或空闲超时数据提交为 InputPacket。
 * English: Serial input adapter; commits a CR/LF-delimited line or idle-timeout data as an InputPacket.
 */
class SerialInputPort : public IInputPort {
 public:
  /**
   * 中文：重置内部缓存并打印串口输入准备信息。
   * English: Resets internal buffers and prints a Serial input ready message.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void begin() override;

  /**
   * 中文：轮询串口，收到一条完整消息时返回 true。
   * English: Polls Serial and returns true when one complete message is available.
   *
   * @param[out] out 中文：输出数据包，指向 ready_ 缓存。
   *                  English: Output packet pointing to the ready_ buffer.
   * @return 中文：true 表示收到完整消息；false 表示没有可处理消息。
   *         English: true when a complete message is available; false otherwise.
   */
  bool poll(InputPacket& out) override;

  /**
   * 中文：清空串口硬件缓冲区和内部缓存。
   * English: Clears both the Serial hardware buffer and internal buffers.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void drain() override;

 private:
  /**
   * 中文：根据换行或空闲超时判断是否把接收缓存提交到 ready_。
   * English: Decides whether to commit the receive buffer into ready_ based on newline or idle timeout.
   *
   * @param force 中文：true 表示强制提交，通常来自 CR/LF；false 表示只在空闲超时后提交。
   *              English: true forces commit, usually from CR/LF; false commits only after idle timeout.
   * @return 中文：true 表示已生成 ready_ 数据；false 表示还没有完整消息。
   *         English: true when ready_ has a packet; false when no complete message exists yet.
   */
  bool commitIfNeeded(bool force);

  /// 中文：正在接收的一条消息缓存。
  /// English: Buffer for the message currently being received.
  uint8_t buffer_[WallEConfig::kInputMaxBytes] = {0};

  /// 中文：已经提交、等待上层读取的消息缓存。
  /// English: Committed message buffer waiting for the upper layer to read.
  uint8_t ready_[WallEConfig::kInputMaxBytes] = {0};

  /// 中文：buffer_ 当前有效字节数。
  /// English: Current valid byte count in buffer_.
  size_t length_ = 0;

  /// 中文：ready_ 当前有效字节数。
  /// English: Current valid byte count in ready_.
  size_t readyLength_ = 0;

  /// 中文：最近一次收到串口字节的 millis() 时间戳。
  /// English: millis() timestamp of the latest received Serial byte.
  uint32_t lastRxMs_ = 0;
};

}  // namespace WallE
