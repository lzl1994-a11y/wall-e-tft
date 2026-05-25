#include "domain/ChatSession.h"

namespace WallE {

/**
 * 中文：向会话末尾追加一条消息；容量满时先删除最旧的一条。
 * English: Appends one message to the session; when capacity is full, removes the oldest one first.
 *
 * @param role 中文：消息角色，决定主屏标签和颜色。
 *             English: Message role that determines main-screen label and color.
 * @param data 中文：消息正文原始字节，通常来自串口 GBK/GB2312 数据。
 *             English: Raw message body bytes, usually GBK/GB2312 data from Serial.
 * @param len 中文：data 的有效字节数。
 *            English: Number of valid bytes in data.
 * @return 无 / None.
 */
void ChatSession::add(ChatRole role, const uint8_t* data, size_t len) {
  if (count_ >= WallEConfig::kChatMaxMessages) {
    // 中文：缓存满时左移数组，保留最近消息。
    // English: Shift the array left when full to keep the newest messages.
    for (size_t i = 1; i < count_; ++i) {
      messages_[i - 1] = messages_[i];
    }
    count_ = WallEConfig::kChatMaxMessages - 1;
  }
  messages_[count_++].set(role, data, len);
}

/**
 * 中文：追加一条以 C 字符串表示的消息；主要用于内部系统提示。
 * English: Appends one C-string message; mainly used for internal system notices.
 *
 * @param role 中文：消息角色。
 *             English: Message role.
 * @param text 中文：以 '\0' 结尾的文本。
 *             English: Null-terminated text.
 * @return 无 / None.
 */
void ChatSession::addText(ChatRole role, const char* text) {
  add(role, reinterpret_cast<const uint8_t*>(text), strlen(text));
}

}  // namespace WallE
