#pragma once

#include <stddef.h>
#include <stdint.h>

namespace WallE {

/**
 * 中文：输入端口返回的一帧数据；当前串口实现把一行数据作为一个包。
 * English: One packet returned by an input port; the current Serial implementation uses one line as one packet.
 */
struct InputPacket {
  /// 中文：数据首地址；指向输入端口内部缓存，下一次 poll() 后可能失效。
  /// English: Data pointer; points to an internal input buffer and may become invalid after the next poll().
  const uint8_t* data = nullptr;

  /// 中文：data 指向的数据长度，单位是字节，不包含结尾 '\0'。
  /// English: Byte length of data; it does not include a trailing '\0'.
  size_t length = 0;
};

/**
 * 中文：输入抽象接口，让应用逻辑不依赖具体输入来源。
 * English: Input abstraction so application logic is independent of the concrete input source.
 */
class IInputPort {
 public:
  /**
   * 中文：虚析构函数，保证通过接口销毁派生类时行为正确。
   * English: Virtual destructor for safe destruction through the interface.
   */
  virtual ~IInputPort() = default;

  /**
   * 中文：初始化输入端口，例如清空缓存或打印准备信息。
   * English: Initializes the input port, for example clearing buffers or printing a ready message.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  virtual void begin() = 0;

  /**
   * 中文：轮询是否收到一个完整输入包。
   * English: Polls whether a complete input packet has been received.
   *
   * @param[out] out 中文：收到数据时写入数据指针和长度。
   *                  English: Receives the data pointer and length when a packet is available.
   * @return 中文：true 表示 out 有效；false 表示当前没有完整数据。
   *         English: true when out is valid; false when no complete packet is available.
   */
  virtual bool poll(InputPacket& out) = 0;

  /**
   * 中文：丢弃当前输入缓存和硬件接收缓冲区中的未处理数据。
   * English: Discards pending bytes in both the internal buffer and hardware receive buffer.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  virtual void drain() = 0;
};

}  // namespace WallE
