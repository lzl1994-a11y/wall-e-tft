#include "adapters/display/St7789DisplayPort.h"
#include "adapters/display/SharedSpiBus.h"
#include "config/Config.h"

namespace WallE {

namespace {
/// 中文：RGB565 黑色。
/// English: RGB565 black.
const uint16_t kBlack = 0x0000;

/// 中文：RGB565 白色，用于正文和边框。
/// English: RGB565 white, used for body text and borders.
const uint16_t kWhite = 0xFFFF;

/// 中文：RGB565 红色，用于启动自检色块。
/// English: RGB565 red, used for startup self-test blocks.
const uint16_t kRed = 0xF800;

/// 中文：RGB565 绿色，用于用户消息和自检色块。
/// English: RGB565 green, used for user messages and self-test blocks.
const uint16_t kGreen = 0x07E0;

/// 中文：RGB565 青色，用于 AI/系统消息。
/// English: RGB565 cyan, used for AI/system messages.
const uint16_t kCyan = 0x07FF;

/// 中文：RGB565 黄色，用于标题文字。
/// English: RGB565 yellow, used for title text.
const uint16_t kYellow = 0xFFE0;

/// 中文：整体背景色，偏暗以降低闪烁感。
/// English: Main background color, kept dark to reduce perceived flicker.
const uint16_t kBg = 0x08A2;

/// 中文：顶部栏、标签背景等面板色。
/// English: Panel color for title bar and role label backgrounds.
const uint16_t kPanel = 0x1966;

/// 中文：UI 分割线颜色。
/// English: UI divider line color.
const uint16_t kLine = 0x4D55;

/// 中文：聊天区域顶部 y 坐标。
/// English: Top y coordinate of the chat area.
constexpr int kChatTop = 32;

/// 中文：聊天区域底部 y 坐标。
/// English: Bottom y coordinate of the chat area.
constexpr int kChatBottom = 208;

/// 中文：聊天区域清屏矩形顶部 y 坐标。
/// English: Top y coordinate of the chat clear rectangle.
constexpr int kChatClearTop = 29;

/// 中文：聊天区域清屏矩形高度。
/// English: Height of the chat clear rectangle.
constexpr int kChatClearHeight = 183;

/// 中文：角色标签左侧 x 坐标。
/// English: Left x coordinate of the role label.
constexpr int kMessageLeft = 8;

/// 中文：消息正文左侧 x 坐标。
/// English: Left x coordinate of the message body text.
constexpr int kMessageTextLeft = 46;

/// 中文：消息正文最大宽度，超出后自动换行。
/// English: Maximum width of message body text; wraps when exceeded.
constexpr int kMessageTextWidth = 186;
constexpr int kPowerBarX = 98;
constexpr int kPowerBarW = 106;
constexpr int kPowerThinH = 9;
constexpr int kPowerThinGap = 8;
constexpr int kPowerThinTopY = 70;
constexpr int kPowerThinCount = 7;
constexpr int kPowerThickY = 194;
constexpr int kPowerThickH = 24;
}  // namespace

/**
 * 中文：初始化主屏模块；先初始化共享 SPI 片选，再初始化 ST7789 和字库 Flash。
 * English: Initializes the main display module; prepares shared SPI CS pins, then initializes ST7789 and font flash.
 *
 * @param 无 / None.
 * @return 中文：主屏初始化成功返回 true；ST7789 初始化失败返回 false。
 *         English: true when the main display initializes successfully; false when ST7789 initialization fails.
 */
bool St7789DisplayPort::begin() {
  beginSharedSpiCsPins();

  if (!initMainDisplay()) {
    return false;
  }

  font_.begin();
  fontOk_ = font_.checkSignature();
  return true;
}

/**
 * 中文：创建 Arduino_GFX 总线和 ST7789 对象，并按配置频率启动屏幕。
 * English: Creates the Arduino_GFX bus and ST7789 object, then starts the display at the configured SPI frequency.
 *
 * @param 无 / None.
 * @return 中文：屏幕对象创建并 begin() 成功返回 true，否则返回 false。
 *         English: true when objects are created and begin() succeeds; false otherwise.
 */
bool St7789DisplayPort::initMainDisplay() {
  deselectSharedSpiDevices();

  // 中文：主屏和字库 Flash 共用 SPI 引脚，因此使用同一套硬件 SPI 配置。
  // English: The main display and font flash share SPI pins, so use the same hardware SPI setup.
  bus_ = new Arduino_HWSPI(WallEConfig::kTftDc, WallEConfig::kTftCs, WallEConfig::kTftSck,
                           WallEConfig::kTftMosi, WallEConfig::kTftMiso);
  gfx_ = new Arduino_ST7789(bus_, WallEConfig::kTftRst, 0, true, WallEConfig::kScreenWidth,
                            WallEConfig::kScreenHeight, 0, 0, 0, 80);
  if (bus_ == nullptr || gfx_ == nullptr) {
    return false;
  }
  if (!gfx_->begin(WallEConfig::kTftSpiHz)) {
    return false;
  }

  gfx_->fillScreen(kBg);
  if (WallEConfig::kMainTftSelfTestOnBoot) {
    drawStartupSelfTest();
  }
  deselectSharedSpiDevices();
  return true;
}

/**
 * 中文：绘制一次启动自检彩条，帮助肉眼确认屏幕供电、SPI、方向和偏移是否正常。
 * English: Draws one boot color-bar self-test to visually confirm power, SPI, orientation, and offsets.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
void St7789DisplayPort::drawStartupSelfTest() {
  if (gfx_ == nullptr) {
    return;
  }

  gfx_->fillRect(0, 0, 80, 240, kRed);
  gfx_->fillRect(80, 0, 80, 240, kGreen);
  gfx_->fillRect(160, 0, 80, 240, 0x001F);
  gfx_->drawRect(0, 0, 240, 240, kWhite);
  gfx_->setTextColor(kWhite);
  gfx_->setTextSize(1);
  gfx_->setCursor(42, 112);
  gfx_->print("ST7789 SPI TEST");
  delay(900);
  gfx_->fillScreen(kBg);
}

/**
 * 中文：只刷新状态栏区域，不影响聊天内容区域。
 * English: Refreshes only the status area without touching the chat content area.
 *
 * @param state 中文：当前应用状态。
 *              English: Current application state.
 * @return 无 / None.
 */
void St7789DisplayPort::showStatus(AppState state) {
  if (gfx_ == nullptr) {
    return;
  }

  deselectSharedSpiDevices();
  gfx_->fillRect(158, 3, 76, 18, kPanel);
  gfx_->setTextColor(kWhite);
  gfx_->setTextSize(1);
  gfx_->setCursor(164, 8);
  gfx_->print(statusText(state));
}

void St7789DisplayPort::showPower(uint8_t percent) {
  if (gfx_ == nullptr) {
    return;
  }

  deselectSharedSpiDevices();
  drawPowerFrame();
  drawPowerBars(percent, true);
  deselectSharedSpiDevices();
}

void St7789DisplayPort::updatePower(uint8_t percent) {
  if (gfx_ == nullptr) {
    return;
  }

  deselectSharedSpiDevices();
  if (!powerFrameDrawn_) {
    drawPowerFrame();
    drawPowerBars(percent, true);
  } else {
    drawPowerBars(percent, false);
  }
  deselectSharedSpiDevices();
}

void St7789DisplayPort::drawPowerFrame() {
  gfx_->fillScreen(kBlack);
  gfx_->setTextColor(kYellow);
  gfx_->setTextSize(2);
  gfx_->setCursor(14, 26);
  gfx_->print("SOLAR CHARGE LEVEL");
  drawSunIcon(53, 86);
  frameDrawn_ = false;
  powerFrameDrawn_ = true;
  lastPowerThinBars_ = -1;
}

void St7789DisplayPort::drawPowerBars(uint8_t percent, bool force) {
  const int thinBars = activeThinBars(percent);
  if (!force && thinBars == lastPowerThinBars_) {
    return;
  }

  for (int i = 0; i < kPowerThinCount; ++i) {
    const bool active = i >= kPowerThinCount - thinBars;
    const bool wasActive =
        lastPowerThinBars_ >= 0 && i >= kPowerThinCount - lastPowerThinBars_;
    if (force || active != wasActive) {
      drawPowerThinBar(i, active);
    }
  }

  if (force) {
    gfx_->fillRoundRect(kPowerBarX, kPowerThickY, kPowerBarW, kPowerThickH, 5,
                        kYellow);
  }
  lastPowerThinBars_ = thinBars;
}

void St7789DisplayPort::drawPowerThinBar(int index, bool active) {
  const int y = kPowerThinTopY + index * (kPowerThinH + kPowerThinGap);
  gfx_->fillRect(kPowerBarX - 1, y - 1, kPowerBarW + 2, kPowerThinH + 2, kBlack);
  if (active) {
    gfx_->fillRoundRect(kPowerBarX, y, kPowerBarW, kPowerThinH, 4, kYellow);
  }
}

void St7789DisplayPort::drawSunIcon(int centerX, int centerY) {
  constexpr int rays[][4] = {
      {0, -29, 0, -20},     {0, 20, 0, 29},      {-29, 0, -20, 0},
      {20, 0, 29, 0},       {-21, -21, -15, -15}, {15, 15, 21, 21},
      {-21, 21, -15, 15},   {15, -15, 21, -21},
  };

  for (size_t i = 0; i < sizeof(rays) / sizeof(rays[0]); ++i) {
    gfx_->drawLine(centerX + rays[i][0], centerY + rays[i][1], centerX + rays[i][2],
                   centerY + rays[i][3], kYellow);
    gfx_->drawLine(centerX + rays[i][0] + 1, centerY + rays[i][1], centerX + rays[i][2] + 1,
                   centerY + rays[i][3], kYellow);
  }

  gfx_->fillCircle(centerX, centerY, 13, kYellow);
  gfx_->fillCircle(centerX, centerY, 7, kBlack);
}

int St7789DisplayPort::activeThinBars(uint8_t percent) const {
  if (percent >= 80) {
    return kPowerThinCount;
  }
  if (percent < 10) {
    return 0;
  }
  // 中文：粗条作为 0-10% 基础段；80% 满格后，每低 10% 少一个细条。
  // English: The thick bar is the 0-10% base segment; below the 80% full level, each 10% drop removes one thin bar.
  const int bars = static_cast<int>(percent / 10) - 1;
  return max(0, min(kPowerThinCount, bars));
}

/**
 * 中文：重绘聊天区域；会先计算每条消息高度，只显示底部能容纳的最新消息。
 * English: Redraws the chat area; first measures each message and shows only the newest messages that fit.
 *
 * @param session 中文：待显示的会话缓存。
 *                English: Session cache to display.
 * @return 无 / None.
 */
void St7789DisplayPort::render(const ChatSession& session) {
  if (gfx_ == nullptr) {
    return;
  }
  if (!frameDrawn_) {
    drawFrame();
  }
  clearChatArea();
  deselectSharedSpiDevices();

  // 中文：count 是本次渲染需要考虑的消息数量。
  // English: count is the number of messages to consider for this render pass.
  const size_t count = session.count();
  if (count == 0) {
    return;
  }

  // 中文：heights 缓存每条消息高度，后面从最新消息向前找可显示范围。
  // English: heights caches each message height, then the renderer walks backward from the newest message to find the visible range.
  int heights[WallEConfig::kChatMaxMessages] = {0};
  for (size_t i = 0; i < count; ++i) {
    heights[i] = measureMessageHeight(session.at(i));
  }

  // 中文：chatHeight 是聊天区域可用高度。
  // English: chatHeight is the available height of the chat area.
  const int chatHeight = kChatBottom - kChatTop;

  // 中文：start 是第一条需要绘制的消息索引，usedHeight 是已占用高度。
  // English: start is the first message index to draw, and usedHeight is the occupied height.
  size_t start = count - 1;
  int usedHeight = 0;
  for (size_t i = count; i > 0; --i) {
    // 中文：index/messageHeight 表示从最新消息向前检查的当前消息。
    // English: index/messageHeight describe the current message while scanning backward from the newest one.
    const size_t index = i - 1;
    const int messageHeight = heights[index];
    if (usedHeight > 0 && usedHeight + messageHeight > chatHeight) {
      break;
    }
    start = index;
    usedHeight += messageHeight;
    if (usedHeight >= chatHeight) {
      break;
    }
  }

  // 中文：y 是当前消息绘制的顶部坐标。
  // English: y is the top coordinate for drawing the current message.
  int y = kChatTop;
  for (size_t i = start; i < count && y < kChatBottom; ++i) {
    drawMessage(session.at(i), y);
  }
  deselectSharedSpiDevices();
}

/**
 * 中文：绘制固定界面框架，包括顶部标题栏、聊天边界和底部说明。
 * English: Draws the fixed UI frame including top title bar, chat boundary, and footer.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
void St7789DisplayPort::drawFrame() {
  deselectSharedSpiDevices();
  gfx_->fillScreen(kBg);
  gfx_->fillRect(0, 0, 240, 28, kPanel);
  gfx_->drawRect(0, 0, 240, 240, kLine);
  gfx_->drawLine(0, 28, 239, 28, kLine);
  gfx_->drawLine(0, 212, 239, 212, kLine);
  gfx_->setTextColor(kYellow);
  gfx_->setTextSize(1);
  gfx_->setCursor(8, 9);
  gfx_->print("WALL-E");
  gfx_->setTextColor(kWhite);
  gfx_->setCursor(8, 222);
  gfx_->print("ESP32 SERIAL DISPLAY");
  frameDrawn_ = true;
  powerFrameDrawn_ = false;
}

/**
 * 中文：清除聊天内容区域，保留固定 UI 框架不变。
 * English: Clears the chat content area while keeping the fixed UI frame unchanged.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
void St7789DisplayPort::clearChatArea() {
  deselectSharedSpiDevices();
  gfx_->fillRect(1, kChatClearTop, 238, kChatClearHeight, kBg);
}

/**
 * 中文：绘制单条消息的角色标签和正文，并更新下一条消息的 y 坐标。
 * English: Draws one message role label and body, then updates the y coordinate for the next message.
 *
 * @param msg 中文：要绘制的消息。
 *            English: Message to draw.
 * @param[in,out] y 中文：输入为本条消息顶部；输出为下一条消息顶部。
 *                  English: Input is this message top; output is next message top.
 * @return 无 / None.
 */
void St7789DisplayPort::drawMessage(const ChatMessage& msg, int& y) {
  if (y + 16 > kChatBottom) {
    y = kChatBottom;
    return;
  }

  // 中文：color 是当前消息角色对应的标签颜色。
  // English: color is the label color selected for the current message role.
  const uint16_t color = roleColor(msg.role);
  gfx_->fillRect(kMessageLeft, y, 32, 16, kPanel);
  gfx_->drawRect(kMessageLeft, y, 32, 16, color);
  gfx_->setTextColor(color);
  gfx_->setTextSize(1);
  gfx_->setCursor(kMessageLeft + 5, y + 4);
  gfx_->print(roleLabel(msg.role));

  // 中文：nextY 接收正文绘制后的底部位置，用于放置下一条消息。
  // English: nextY receives the body bottom position and is used to place the next message.
  int nextY = y;
  drawBytes(kMessageTextLeft, y, msg.data, msg.length, kWhite, kBg, kMessageTextWidth,
            kChatBottom, nextY);
  y = max(nextY, y + 18) + 4;
}

/**
 * 中文：按字节绘制 GB2312/ASCII 混合文本，自动换行并裁剪到聊天区域底部。
 * English: Draws mixed GB2312/ASCII byte text with wrapping and clipping to the chat area bottom.
 *
 * @param x 中文：文本区域左上角 x。
 *          English: Top-left x of the text area.
 * @param y 中文：文本区域左上角 y。
 *          English: Top-left y of the text area.
 * @param data 中文：文本原始字节。
 *             English: Raw text bytes.
 * @param len 中文：文本字节长度。
 *            English: Text byte length.
 * @param color 中文：前景颜色。
 *              English: Foreground color.
 * @param bg 中文：背景颜色。
 *           English: Background color.
 * @param maxWidth 中文：文本区域最大宽度。
 *                 English: Maximum text area width.
 * @param bottomY 中文：允许绘制的底部边界。
 *                English: Bottom boundary allowed for drawing.
 * @param[out] nextY 中文：输出绘制后的下一行 y。
 *                   English: Output next y after drawing.
 * @return 无 / None.
 */
void St7789DisplayPort::drawBytes(int x, int y, const uint8_t* data, size_t len,
                                  uint16_t color, uint16_t bg, int maxWidth, int bottomY,
                                  int& nextY) {
  // 中文：cursorX/cursorY 是当前字形绘制位置。
  // English: cursorX/cursorY are the draw position of the current glyph.
  int cursorX = x;
  int cursorY = y;

  // 中文：bitmap 是从字库 Flash 读取的单个字符点阵缓存。
  // English: bitmap stores one glyph bitmap read from the font flash.
  uint8_t bitmap[32] = {0};

  for (size_t i = 0; i < len;) {
    // 中文：b 是当前待解析字节，可能是 ASCII，也可能是 GB2312 双字节的高字节。
    // English: b is the current byte, either ASCII or the high byte of a GB2312 two-byte character.
    const uint8_t b = data[i];
    if (b == '\r' || b == '\n') {
      cursorX = x;
      cursorY += 16;
      if (cursorY + 16 > bottomY) {
        break;
      }
      ++i;
      continue;
    }

    // 中文：GB2312/GBK 中文通常是两个 >=0xA1 的字节；否则按 ASCII 处理。
    // English: GB2312/GBK Chinese text usually uses two bytes >= 0xA1; otherwise it is treated as ASCII.
    bool isWord = b >= 0xA1 && i + 1 < len && data[i + 1] >= 0xA1;
    const int w = isWord ? 16 : 8;
    if (cursorX != x && cursorX + w > x + maxWidth) {
      cursorX = x;
      cursorY += 16;
      if (cursorY + 16 > bottomY) {
        break;
      }
    }

    if (cursorY + 16 > bottomY) {
      break;
    }

    if (isWord) {
      font_.readWord16(b, data[i + 1], bitmap);
      drawGlyph(cursorX, cursorY, 16, 16, bitmap, color, bg);
      i += 2;
    } else {
      font_.readAscii16(b, bitmap);
      drawGlyph(cursorX, cursorY, 8, 16, bitmap, color, bg);
      i += 1;
    }
    cursorX += w;
  }
  nextY = min(cursorY + 16, bottomY);
}

/**
 * 中文：绘制一个 1-bit 点阵字形，支持 8x16 ASCII 和 16x16 汉字。
 * English: Draws one 1-bit bitmap glyph, supporting 8x16 ASCII and 16x16 Chinese glyphs.
 *
 * @param x 中文：绘制位置 x。
 *          English: Draw x coordinate.
 * @param y 中文：绘制位置 y。
 *          English: Draw y coordinate.
 * @param w 中文：字形宽度。
 *          English: Glyph width.
 * @param h 中文：字形高度。
 *          English: Glyph height.
 * @param bitmap 中文：1-bit 点阵数据。
 *               English: 1-bit bitmap data.
 * @param color 中文：点亮像素颜色。
 *              English: Lit pixel color.
 * @param bg 中文：未点亮像素颜色。
 *           English: Unlit pixel color.
 * @return 无 / None.
 */
void St7789DisplayPort::drawGlyph(int x, int y, int w, int h, const uint8_t* bitmap,
                                  uint16_t color, uint16_t bg) {
  // 中文：rowBytes 是每一行点阵占用的字节数。
  // English: rowBytes is the number of bitmap bytes used by one glyph row.
  const int rowBytes = (w + 7) / 8;

  // 中文：pixels 是转换后的 RGB565 临时像素块，最大支持 16x16。
  // English: pixels is the converted RGB565 temporary block, up to 16x16.
  uint16_t pixels[16 * 16] = {0};
  for (int row = 0; row < h; ++row) {
    for (int col = 0; col < w; ++col) {
      const bool dot = bitmap[row * rowBytes + (col >> 3)] & (0x80 >> (col & 7));
      pixels[row * w + col] = dot ? color : bg;
    }
  }
  deselectSharedSpiDevices();
  gfx_->draw16bitRGBBitmap(x, y, pixels, w, h);
}

/**
 * 中文：测量一条消息的整体高度，包括标签最小高度和消息间距。
 * English: Measures total message height including label minimum height and spacing.
 *
 * @param msg 中文：待测量消息。
 *            English: Message to measure.
 * @return 中文：消息高度，单位像素。
 *         English: Message height in pixels.
 */
int St7789DisplayPort::measureMessageHeight(const ChatMessage& msg) const {
  return max(measureBytesHeight(msg.data, msg.length, kMessageTextWidth), 18) + 4;
}

/**
 * 中文：仅按字节和宽度估算文本换行高度，不访问屏幕。
 * English: Estimates wrapped text height from bytes and width only, without touching the display.
 *
 * @param data 中文：文本原始字节。
 *             English: Raw text bytes.
 * @param len 中文：字节长度。
 *            English: Byte length.
 * @param maxWidth 中文：最大宽度。
 *                 English: Maximum width.
 * @return 中文：估算高度，单位像素。
 *         English: Estimated height in pixels.
 */
int St7789DisplayPort::measureBytesHeight(const uint8_t* data, size_t len, int maxWidth) const {
  if (data == nullptr || len == 0) {
    return 16;
  }

  // 中文：cursorX 只用于测量当前行已占宽度，不对应真实屏幕 x 坐标。
  // English: cursorX only measures the used width on the current line; it is not a real screen x coordinate.
  int cursorX = 0;

  // 中文：lines 是自动换行后的文本行数。
  // English: lines is the number of text lines after wrapping.
  int lines = 1;
  for (size_t i = 0; i < len;) {
    // 中文：b 是当前测量字节；测量逻辑必须和 drawBytes() 的字宽规则一致。
    // English: b is the current byte for measurement; the width rule must match drawBytes().
    const uint8_t b = data[i];
    if (b == '\r' || b == '\n') {
      cursorX = 0;
      ++lines;
      ++i;
      continue;
    }

    const bool isWord = b >= 0xA1 && i + 1 < len && data[i + 1] >= 0xA1;
    const int w = isWord ? 16 : 8;
    if (cursorX != 0 && cursorX + w > maxWidth) {
      cursorX = 0;
      ++lines;
    }

    cursorX += w;
    i += isWord ? 2 : 1;
  }
  return lines * 16;
}

/**
 * 中文：把 AppState 枚举转换为短状态文本。
 * English: Converts an AppState enum into short status text.
 *
 * @param state 中文：应用状态。
 *              English: Application state.
 * @return 中文：状态栏使用的静态字符串。
 *         English: Static string used by the status bar.
 */
const char* St7789DisplayPort::statusText(AppState state) const {
  switch (state) {
    case AppState::Booting:
      return "BOOT";
    case AppState::Ready:
      return "READY";
  }
  return "?";
}

/**
 * 中文：返回指定角色对应的 UI 颜色。
 * English: Returns the UI color for a message role.
 *
 * @param role 中文：消息角色。
 *             English: Message role.
 * @return 中文：RGB565 颜色。
 *         English: RGB565 color.
 */
uint16_t St7789DisplayPort::roleColor(ChatRole role) const {
  switch (role) {
    case ChatRole::User:
      return kGreen;
    case ChatRole::Assistant:
      return kCyan;
    case ChatRole::System:
      return kCyan;
  }
  return kWhite;
}

/**
 * 中文：返回指定角色在左侧标签中显示的短文本。
 * English: Returns the short label text shown for a message role.
 *
 * @param role 中文：消息角色。
 *             English: Message role.
 * @return 中文：静态标签字符串。
 *         English: Static label string.
 */
const char* St7789DisplayPort::roleLabel(ChatRole role) const {
  switch (role) {
    case ChatRole::User:
      return "YOU";
    case ChatRole::Assistant:
      return "AI";
    case ChatRole::System:
      return "SYS";
  }
  return "?";
}

}  // namespace WallE
