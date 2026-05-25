#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

namespace WallE {

/**
 * 中文：一条聊天消息的角色，用于决定标签和颜色。
 * English: Role of one chat message, used to choose label and color.
 */
enum class ChatRole : uint8_t {
  /// 中文：用户发送的内容，对应串口前缀 you:。
  /// English: User content, mapped from the serial prefix you:.
  User,

  /// 中文：AI 回复内容，对应串口前缀 ai:。
  /// English: AI response content, mapped from the serial prefix ai:.
  Assistant,

  /// 中文：系统提示，例如 READY 或 FONT OK。
  /// English: System notices such as READY or FONT OK.
  System,

};

/**
 * 中文：会话中的一条消息；字节内容按 GBK/GB2312 原样保存。
 * English: One message in the chat session; bytes are stored as-is in GBK/GB2312 form.
 */
struct ChatMessage {
  /// 中文：消息角色，决定 ST7789 上的标签和颜色。
  /// English: Message role that determines label and color on the ST7789 display.
  ChatRole role = ChatRole::System;

  /// 中文：消息原始字节缓存；不要求是 UTF-8，也不自动补 '\0'。
  /// English: Raw message byte buffer; it is not required to be UTF-8 and is not automatically null-terminated.
  uint8_t data[1024] = {0};

  /// 中文：data 中有效字节数。
  /// English: Number of valid bytes stored in data.
  size_t length = 0;

  /**
   * 中文：设置消息角色和原始字节内容，超出内部缓存的部分会被截断。
   * English: Sets the role and raw byte content; data longer than the internal buffer is truncated.
   *
   * @param nextRole 中文：新的消息角色。
   *                 English: New message role.
   * @param bytes 中文：待复制的字节指针；正常调用应传入有效指针。
   *              English: Byte pointer to copy; normal calls should pass a valid pointer.
   * @param len 中文：待复制的字节长度。
   *            English: Number of bytes to copy.
   * @return 无 / None.
   */
  void set(ChatRole nextRole, const uint8_t* bytes, size_t len) {
    role = nextRole;
    length = min(len, sizeof(data));
    if (bytes != nullptr && length > 0) {
      memcpy(data, bytes, length);
    }
  }

};

}  // namespace WallE
