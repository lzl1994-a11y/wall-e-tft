#pragma once

#include "config/Config.h"
#include "domain/ChatMessage.h"

namespace WallE {

/**
 * 中文：主屏显示的聊天会话缓存；容量固定，满了会滚动丢弃最旧消息。
 * English: Fixed-size chat session buffer for the main display; when full it scrolls out the oldest message.
 */
class ChatSession {
 public:
  /**
   * 中文：添加一条原始字节消息。
   * English: Adds one raw-byte message.
   *
   * @param role 中文：消息角色，用于 UI 区分用户、AI 和系统消息。
   *             English: Message role used by UI to distinguish user, AI, and system messages.
   * @param data 中文：原始字节指针，通常是 GBK/GB2312 内容。
   *             English: Raw byte pointer, usually GBK/GB2312 content.
   * @param len 中文：data 的字节长度。
   *            English: Byte length of data.
   * @return 无 / None.
   */
  void add(ChatRole role, const uint8_t* data, size_t len);

  /**
   * 中文：添加一条 C 字符串消息，适合内部系统提示。
   * English: Adds one C-string message, suitable for internal system notices.
   *
   * @param role 中文：消息角色。
   *             English: Message role.
   * @param text 中文：以 '\0' 结尾的文本。
   *             English: Null-terminated text.
   * @return 无 / None.
   */
  void addText(ChatRole role, const char* text);

  /**
   * 中文：获取当前消息数量。
   * English: Gets the current number of messages.
   *
   * @param 无 / None.
   * @return 中文：messages_ 中有效消息数量。
   *         English: Number of valid messages in messages_.
   */
  size_t count() const { return count_; }

  /**
   * 中文：按索引读取消息；调用方需保证 index < count()。
   * English: Reads a message by index; caller must ensure index < count().
   *
   * @param index 中文：消息索引，从 0 开始。
   *              English: Zero-based message index.
   * @return 中文：指定消息的只读引用。
   *         English: Const reference to the selected message.
   */
  const ChatMessage& at(size_t index) const { return messages_[index]; }

 private:
  /// 中文：固定容量消息数组，容量由 kChatMaxMessages 配置。
  /// English: Fixed-capacity message array sized by kChatMaxMessages.
  ChatMessage messages_[WallEConfig::kChatMaxMessages];

  /// 中文：当前有效消息数量。
  /// English: Current number of valid messages.
  size_t count_ = 0;
};

}  // namespace WallE
