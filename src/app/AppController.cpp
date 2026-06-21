#include "app/AppController.h"
#include <Wire.h>
namespace WallE {

namespace {

/**
 * 中文：判断字节数组是否以指定 ASCII 前缀开头，比较时忽略大小写。
 * English: Checks whether a byte array starts with an ASCII prefix, ignoring case.
 *
 * @param data 中文：待检查的原始字节数据。
 *             English: Raw bytes to check.
 * @param len 中文：data 的字节长度。
 *            English: Byte length of data.
 * @param prefix 中文：以 '\0' 结尾的 ASCII 前缀。
 *               English: Null-terminated ASCII prefix.
 * @return 中文：匹配前缀返回 true，否则返回 false。
 *         English: true when the prefix matches; false otherwise.
 */
bool startsWithIgnoreCase(const uint8_t* data, size_t len, const char* prefix) {
  if (data == nullptr || prefix == nullptr) {
    return false;
  }

  for (size_t i = 0; prefix[i] != '\0'; ++i) {
    if (i >= len) {
      return false;
    }

    // 中文：received/expected 都是 ASCII 命令字符，只做英文大小写折叠，不碰中文正文。
    // English: received/expected are ASCII command characters; only English case folding is applied and message body bytes are untouched.
    char received = static_cast<char>(data[i]);
    char expected = prefix[i];
    if (received >= 'A' && received <= 'Z') {
      received = static_cast<char>(received - 'A' + 'a');
    }
    if (expected >= 'A' && expected <= 'Z') {
      expected = static_cast<char>(expected - 'A' + 'a');
    }
    if (received != expected) {
      return false;
    }
  }
  return true;
}

/**
 * 中文：判断字节数组是否和指定 ASCII 文本完全相同，比较时忽略大小写。
 * English: Checks whether a byte array exactly equals an ASCII text, ignoring case.
 *
 * @param data 中文：待比较的原始字节数据。
 *             English: Raw bytes to compare.
 * @param len 中文：data 的字节长度。
 *            English: Byte length of data.
 * @param text 中文：以 '\0' 结尾的 ASCII 文本。
 *             English: Null-terminated ASCII text.
 * @return 中文：完全相同返回 true，否则返回 false。
 *         English: true when exactly equal; false otherwise.
 */
bool equalsIgnoreCase(const uint8_t* data, size_t len, const char* text) {
  if (data == nullptr || text == nullptr) {
    return false;
  }

  size_t i = 0;
  for (; i < len && text[i] != '\0'; ++i) {
    // 中文：这里只用于比较动作名 zoom 等 ASCII 关键字。
    // English: This comparison is only for ASCII keywords such as the zoom action name.
    char received = static_cast<char>(data[i]);
    char expected = text[i];
    if (received >= 'A' && received <= 'Z') {
      received = static_cast<char>(received - 'A' + 'a');
    }
    if (expected >= 'A' && expected <= 'Z') {
      expected = static_cast<char>(expected - 'A' + 'a');
    }
    if (received != expected) {
      return false;
    }
  }
  return i == len && text[i] == '\0';
}

/**
 * 中文：去掉命令参数前后的 ASCII 空格和制表符，不改动内容缓冲区本身。
 * English: Trims ASCII spaces and tabs around a command argument without modifying the underlying buffer.
 *
 * @param[in,out] data 中文：输入为原始参数起点，输出为修剪后的起点。
 *                     English: Input is the raw argument start; output is the trimmed start.
 * @param[in,out] length 中文：输入为原始长度，输出为修剪后的长度。
 *                       English: Input is the raw length; output is the trimmed length.
 * @return 无 / None.
 */
void trimAsciiWhitespace(const uint8_t*& data, size_t& length) {
  while (length > 0 && (data[0] == ' ' || data[0] == '\t')) {
    ++data;
    --length;
  }

  while (length > 0) {
    // 中文：tail 是当前参数最后一个字节，只移除 ASCII 空格/制表符。
    // English: tail is the last byte of the current argument; only ASCII space/tab is trimmed.
    const uint8_t tail = data[length - 1];
    if (tail != ' ' && tail != '\t') {
      break;
    }
    --length;
  }
}

/**
 * 中文：解析眼睛动作命令；当前格式是 eyeaction:zoom。
 * English: Parses an eye action command; the current format is eyeaction:zoom.
 *
 * @param packet 中文：串口输入包。
 *               English: Serial input packet.
 * @param[out] actionData 中文：解析成功时指向动作名起点，例如 zoom。
 *                        English: On success, points to the action name such as zoom.
 * @param[out] actionLength 中文：动作名长度。
 *                          English: Length of the action name.
 * @return 中文：是 eyeaction 命令且动作名非空时返回 true。
 *         English: true when this is an eyeaction command with a non-empty action name.
 */
bool parseEyeAction(const InputPacket& packet, const uint8_t*& actionData, size_t& actionLength) {
  // 中文：actionData/actionLength 初始指向完整输入包，确认前缀后再移动到动作名。
  // English: actionData/actionLength first point to the whole packet and move to the action name after prefix detection.
  actionData = packet.data;
  actionLength = packet.length;
  if (actionData == nullptr || actionLength == 0) {
    return false;
  }

  if (!startsWithIgnoreCase(actionData, actionLength, "eyeaction:")) {
    return false;
  }

  actionData += 10;
  actionLength -= 10;
  trimAsciiWhitespace(actionData, actionLength);
  return actionLength > 0;
}

/**
 * 中文：解析已经确认前缀的非负整数命令参数，例如 power:80 / openchat:1。
 * English: Parses the numeric argument of a prefix-matched command, such as power:80 / openchat:1.
 *
 * @param packet 中文：串口输入包。
 *               English: Serial input packet.
 * @param prefix 中文：命令前缀，例如 power:。
 *               English: Command prefix, for example power:.
 * @param[out] value 中文：解析出的整数值。
 *                   English: Parsed integer value.
 * @return 中文：参数存在且全部为数字时返回 true。
 *         English: true when the argument exists and is numeric.
 */
bool parseNumberAfterPrefix(const InputPacket& packet, const char* prefix, int& value) {
  const uint8_t* data = packet.data;
  size_t length = packet.length;
  if (data == nullptr || length == 0 || prefix == nullptr) {
    return false;
  }

  size_t prefixLength = 0;
  while (prefix[prefixLength] != '\0') {
    ++prefixLength;
  }
  if (length <= prefixLength) {
    return false;
  }
  data += prefixLength;
  length -= prefixLength;
  trimAsciiWhitespace(data, length);
  if (length == 0) {
    return false;
  }

  int parsed = 0;
  for (size_t i = 0; i < length; ++i) {
    if (data[i] < '0' || data[i] > '9') {
      return false;
    }
    parsed = parsed * 10 + (data[i] - '0');
    if (parsed > 1000) {
      return false;
    }
  }

  value = parsed;
  return true;
}

/**
 * 中文：解析主屏聊天消息；支持 you: 和 ai: 前缀，未带前缀时默认当作用户消息。
 * English: Parses a main-display chat message; supports you: and ai: prefixes, and defaults to user text without a prefix.
 *
 * @param packet 中文：串口输入包。
 *               English: Serial input packet.
 * @param[out] role 中文：解析出的消息角色。
 *                  English: Parsed message role.
 * @param[out] data 中文：解析后的正文起点，仍指向原始字节，保持 GBK/GB2312 不转换。
 *                  English: Parsed body start, still pointing to raw bytes to preserve GBK/GB2312 without conversion.
 * @param[out] length 中文：正文长度。
 *                    English: Body length.
 * @return 中文：正文非空时返回 true，否则返回 false。
 *         English: true when the body is non-empty; false otherwise.
 */
bool parseSerialMessage(const InputPacket& packet, ChatRole& role, const uint8_t*& data,
                        size_t& length) {
  // 中文：未带前缀时默认当成用户消息，便于简单串口调试。
  // English: Without an explicit prefix, the packet defaults to a user message for easier Serial testing.
  role = ChatRole::User;
  data = packet.data;
  length = packet.length;

  if (data == nullptr || length == 0) {
    return false;
  }
  if (startsWithIgnoreCase(data, length, "you:")) {
    data += 4;
    length -= 4;
  } else if (startsWithIgnoreCase(data, length, "ai:")) {
    role = ChatRole::Assistant;
    data += 3;
    length -= 3;
  }

  // 中文：允许 "you: 内容" 这种格式，但不修改中文正文的原始编码。
  // English: Accept "you: text" while keeping the body bytes in their original encoding.
  if (length > 0 && *data == ' ') {
    ++data;
    --length;
  }
  return length > 0;
}

/**
 * 中文：解析 pca9685: 逗号分隔的 15 个整数。
 * English: Parses 15 comma-separated integers following pca9685: prefix.
 */
bool parsePca9685(const InputPacket& packet, int16_t* values) {
  const uint8_t* data = packet.data;
  size_t length = packet.length;
  
  if (!startsWithIgnoreCase(data, length, "pca9685:")) {
    return false;
  }
  
  data += 8;
  length -= 8;
  
  size_t count = 0;
  int currentVal = 0;
  bool inNumber = false;
  bool isNegative = false;
  
  for (size_t i = 0; i <= length; ++i) {
    char c = (i < length) ? static_cast<char>(data[i]) : ',';
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      continue;
    }
    
    if (c == '-') {
      if (inNumber) return false;
      isNegative = true;
      inNumber = true;
    } else if (c >= '0' && c <= '9') {
      currentVal = currentVal * 10 + (c - '0');
      inNumber = true;
    } else if (c == ',') {
      if (!inNumber) return false;
      if (count < 15) {
        values[count++] = static_cast<int16_t>(isNegative ? -currentVal : currentVal);
      } else {
        return false;
      }
      currentVal = 0;
      inNumber = false;
      isNegative = false;
    } else {
      return false;
    }
  }
  
  return count == 15;
}

}  // namespace

/**
 * 中文：构造控制器并保存各端口引用；不拥有这些对象的生命周期。
 * English: Constructs the controller and stores port references; it does not own these objects.
 *
 * @param logger 中文：日志输出端口。
 *               English: Logger output port.
 * @param input 中文：串口输入端口。
 *              English: Serial input port.
 * @param display 中文：主屏显示端口。
 *                English: Main display port.
 * @param eyeDisplay 中文：眼睛屏显示端口。
 *                   English: Eye display port.
 */
AppController::AppController(ILogger& logger, IInputPort& input, IDisplayPort& display,
                             IEyeDisplayPort& eyeDisplay)
    : logger_(logger), input_(input), display_(display), eyeDisplay_(eyeDisplay) {}

/**
 * 中文：启动应用；初始化日志、主屏、输入端口，并在主屏显示 READY 状态。
 * English: Starts the application; initializes logger, main display, input port, and shows READY on the main display.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
void AppController::begin() {
  logger_.begin();
  logger_.info("Wall-E Arduino serial display boot");

  if (!display_.begin()) {
    logger_.error("display init failed");
  }
  setState(AppState::Booting);
  session_.addText(ChatRole::System, display_.fontOk() ? "TEXT OK" : "TEXT ERR");

  input_.begin();
  session_.addText(ChatRole::System, "READY");
  setState(AppState::Ready);
  display_.showPower(powerPercent_);

  Wire.begin(WallEConfig::kPca9685Sda, WallEConfig::kPca9685Scl);
  Wire.beginTransmission(0x40);
  Wire.write(0x00);
  Wire.write(0x20); // Enable auto-increment
  Wire.endTransmission();
  
  bootMs_ = millis();
  pca9685Rx_ = false;
}

/**
 * 中文：应用循环；读取一条串口输入并根据前缀分发到主屏或眼睛屏。
 * English: Application loop; reads one Serial input packet and dispatches it to the main display or eye display by prefix.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
void AppController::loop() {
  eyeDisplay_.update();

  // 中文：每次循环最多处理一条完整输入包，避免一次刷新过久。
  // English: Each loop handles at most one complete packet to avoid long redraw bursts.
  InputPacket packet;
  if (!input_.poll(packet)) {
    returnToPowerIfChatIdle();
    
    if (!pca9685Rx_ && (millis() - bootMs_ > 5000)) {
      pca9685Rx_ = true;
      logger_.info("pca9685: 5s timeout, applying default neutral/stop");
      
      Wire.beginTransmission(0x40);
      Wire.write(0x06); // Start at LED0_ON_L
      for (int i = 0; i < 15; ++i) {
        int16_t val = (i <= 8) ? 4915 : 0;
        Wire.write(0);
        Wire.write(0);
        Wire.write(val & 0xFF);
        Wire.write((val >> 8) & 0xFF);
      }
      Wire.endTransmission();
    }
    
    delay(10);
    return;
  }

  if (startsWithIgnoreCase(packet.data, packet.length, "pca9685:")) {
    int16_t values[15];
    if (parsePca9685(packet, values)) {
      pca9685Rx_ = true;
      Wire.beginTransmission(0x40);
      Wire.write(0x06); // Start at LED0_ON_L
      for (int i = 0; i < 15; ++i) {
        Wire.write(0);
        Wire.write(0);
        Wire.write(values[i] & 0xFF);
        Wire.write((values[i] >> 8) & 0xFF);
      }
      Wire.endTransmission();
    }
    return;
  }

  if (equalsIgnoreCase(packet.data, packet.length, "getname:WHO_ARE_YOU")) {
    Serial.println("IAM:WALL_E_TFT");
    return;
  }

  if (state_ != AppState::Ready) {
    input_.drain();
    logger_.warn("input dropped while not ready");
    return;
  }

  int numberValue = 0;
  if (startsWithIgnoreCase(packet.data, packet.length, "power:")) {
    if (!parseNumberAfterPrefix(packet, "power:", numberValue)) {
      logger_.warn("invalid power command");
      return;
    }
    if (numberValue > 100) {
      numberValue = 100;
    }
    powerPercent_ = static_cast<uint8_t>(numberValue);
    if (screenMode_ == ScreenMode::Power) {
      display_.updatePower(powerPercent_);
    }
    return;
  }

  if (startsWithIgnoreCase(packet.data, packet.length, "openchat:")) {
    if (!parseNumberAfterPrefix(packet, "openchat:", numberValue)) {
      logger_.warn("invalid openchat command");
      return;
    }
    setChatOpen(numberValue == 1);
    return;
  }

  // 中文：eyeActionData/eyeActionLength 保存解析后的眼睛动作名，例如 zoom。
  // English: eyeActionData/eyeActionLength store the parsed eye action name such as zoom.
  const uint8_t* eyeActionData = nullptr;
  size_t eyeActionLength = 0;
  if (parseEyeAction(packet, eyeActionData, eyeActionLength)) {
    if (equalsIgnoreCase(eyeActionData, eyeActionLength, "zoom")) {
      logger_.info("eye action: zoom");
      eyeDisplay_.playZoom();
    } else {
      logger_.warn("unknown eye action");
    }
    return;
  }

  // 中文：role/messageData/messageLength 保存解析后的主屏聊天消息。
  // English: role/messageData/messageLength store the parsed chat message for the main display.
  ChatRole role = ChatRole::User;
  const uint8_t* messageData = nullptr;
  size_t messageLength = 0;
  if (!parseSerialMessage(packet, role, messageData, messageLength)) {
    return;
  }

  if (screenMode_ != ScreenMode::Chat) {
    logger_.warn("chat message dropped while chat closed");
    return;
  }

  lastChatActivityMs_ = millis();
  session_.add(role, messageData, messageLength);
  display_.render(session_);
  setState(AppState::Ready);
}

/**
 * 中文：更新控制器状态并立即刷新主屏状态栏。
 * English: Updates the controller state and immediately refreshes the main display status bar.
 *
 * @param state 中文：新的应用状态。
 *              English: New application state.
 * @return 无 / None.
 */
void AppController::setState(AppState state) {
  state_ = state;
  if (screenMode_ == ScreenMode::Chat) {
    display_.showStatus(state_);
  }
}

void AppController::setChatOpen(bool open) {
  if (open) {
    screenMode_ = ScreenMode::Chat;
    lastChatActivityMs_ = millis();
    display_.render(session_);
    display_.showStatus(state_);
    return;
  }

  screenMode_ = ScreenMode::Power;
  display_.showPower(powerPercent_);
}

void AppController::returnToPowerIfChatIdle() {
  if (screenMode_ != ScreenMode::Chat) {
    return;
  }

  if (millis() - lastChatActivityMs_ >= WallEConfig::kChatIdleReturnMs) {
    setChatOpen(false);
  }
}

}  // namespace WallE
