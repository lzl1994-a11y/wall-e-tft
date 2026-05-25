#pragma once

#include "adapters/display/Gb2312FontFlash.h"
#include "ports/IDisplayPort.h"
#include <Arduino_GFX_Library.h>

namespace WallE {

/**
 * 中文：胸前 ST7789 主屏显示适配器，负责绘制启动页、状态栏和聊天会话。
 * English: Chest ST7789 main display adapter that draws boot page, status bar, and chat session.
 */
class St7789DisplayPort : public IDisplayPort {
 public:
  /**
   * 中文：初始化 ST7789 屏幕和外置 GB2312 字库 Flash。
   * English: Initializes the ST7789 display and the external GB2312 font flash.
   *
   * @param 无 / None.
   * @return 中文：true 表示屏幕初始化成功；false 表示屏幕初始化失败。
   *         English: true when display initialization succeeds; false on display initialization failure.
   */
  bool begin() override;

  /**
   * 中文：返回字库 Flash 签名校验结果。
   * English: Returns the font flash signature verification result.
   *
   * @param 无 / None.
   * @return 中文：true 表示字库可用；false 表示不可用。
   *         English: true when the font is available; false otherwise.
   */
  bool fontOk() const override { return fontOk_; }

  /**
   * 中文：刷新右上角状态栏。
   * English: Refreshes the top-right status area.
   *
   * @param state 中文：应用状态。
   *              English: Application state.
   * @return 无 / None.
   */
  void showStatus(AppState state) override;
  void showPower(uint8_t percent) override;
  void updatePower(uint8_t percent) override;

  /**
   * 中文：根据会话缓存重绘聊天区域，只显示能放进会话区域的最新消息。
   * English: Redraws the chat area from the session cache, showing only the latest messages that fit.
   *
   * @param session 中文：会话缓存，只读。
   *                English: Read-only session cache.
   * @return 无 / None.
   */
  void render(const ChatSession& session) override;

 private:
  /**
   * 中文：初始化主屏硬件、SPI 总线和 ST7789 控制器。
   * English: Initializes main display hardware, SPI bus, and ST7789 controller.
   *
   * @param 无 / None.
   * @return 中文：true 表示初始化成功；false 表示失败。
   *         English: true on success; false on failure.
   */
  bool initMainDisplay();

  /**
   * 中文：绘制红绿蓝彩条自检画面，用于确认屏幕接线和方向。
   * English: Draws an RGB color-bar self-test to verify wiring and orientation.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void drawStartupSelfTest();

  /**
   * 中文：绘制固定 UI 框架，包括标题栏、会话边框和底部说明。
   * English: Draws the fixed UI frame including title bar, chat border, and footer.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void drawFrame();
  void drawPowerFrame();
  void drawPowerBars(uint8_t percent, bool force);
  void drawPowerThinBar(int index, bool active);
  void drawSunIcon(int centerX, int centerY);
  int activeThinBars(uint8_t percent) const;

  /**
   * 中文：清空聊天内容区域，不重绘标题栏和底部栏。
   * English: Clears only the chat content area, leaving title and footer bars intact.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void clearChatArea();

  /**
   * 中文：绘制一条消息，包括左侧角色标签和右侧文本。
   * English: Draws one message with a role label on the left and text on the right.
   *
   * @param msg 中文：待绘制消息。
   *            English: Message to draw.
   * @param[in,out] y 中文：输入为起始 y 坐标，输出为下一条消息的 y 坐标。
   *                  English: Input is start y; output is y for the next message.
   * @return 无 / None.
   */
  void drawMessage(const ChatMessage& msg, int& y);

  /**
   * 中文：绘制原始字节文本；ASCII 按 8x16，GB2312 双字节按 16x16 处理。
   * English: Draws raw byte text; ASCII is treated as 8x16 and GB2312 two-byte text as 16x16.
   *
   * @param x 中文：文本起始 x 坐标。
   *          English: Text start x coordinate.
   * @param y 中文：文本起始 y 坐标。
   *          English: Text start y coordinate.
   * @param data 中文：原始文本字节。
   *             English: Raw text bytes.
   * @param len 中文：文本字节长度。
   *            English: Text byte length.
   * @param color 中文：文字颜色，RGB565。
   *              English: Text color in RGB565.
   * @param bg 中文：背景颜色，RGB565。
   *           English: Background color in RGB565.
   * @param maxWidth 中文：允许绘制的最大宽度。
   *                 English: Maximum drawable width.
   * @param bottomY 中文：聊天区域底部 y 坐标，超出后停止绘制。
   *                English: Bottom y coordinate of chat area; drawing stops beyond it.
   * @param[out] nextY 中文：输出文本绘制后的下一行 y 坐标。
   *                   English: Output y coordinate after the drawn text.
   * @return 无 / None.
   */
  void drawBytes(int x, int y, const uint8_t* data, size_t len, uint16_t color, uint16_t bg,
                 int maxWidth, int bottomY, int& nextY);

  /**
   * 中文：把字库点阵转换为 RGB565 像素并推送到屏幕。
   * English: Converts a bitmap glyph to RGB565 pixels and pushes it to the display.
   *
   * @param x 中文：字形左上角 x 坐标。
   *          English: Glyph top-left x coordinate.
   * @param y 中文：字形左上角 y 坐标。
   *          English: Glyph top-left y coordinate.
   * @param w 中文：字形宽度，像素。
   *          English: Glyph width in pixels.
   * @param h 中文：字形高度，像素。
   *          English: Glyph height in pixels.
   * @param bitmap 中文：字库读取到的 1-bit 点阵数据。
   *               English: 1-bit glyph bitmap read from font flash.
   * @param color 中文：点亮像素颜色。
   *              English: Foreground pixel color.
   * @param bg 中文：未点亮像素背景色。
   *           English: Background color for unset pixels.
   * @return 无 / None.
   */
  void drawGlyph(int x, int y, int w, int h, const uint8_t* bitmap, uint16_t color,
                 uint16_t bg);

  /**
   * 中文：估算一条消息在聊天区域中占用的高度。
   * English: Estimates the height occupied by one message in the chat area.
   *
   * @param msg 中文：待测量消息。
   *            English: Message to measure.
   * @return 中文：消息高度，单位像素，包含消息间距。
   *         English: Message height in pixels, including spacing.
   */
  int measureMessageHeight(const ChatMessage& msg) const;

  /**
   * 中文：估算一段原始字节文本自动换行后的高度。
   * English: Estimates wrapped height for a raw byte text block.
   *
   * @param data 中文：原始文本字节。
   *             English: Raw text bytes.
   * @param len 中文：文本字节长度。
   *            English: Text byte length.
   * @param maxWidth 中文：最大绘制宽度。
   *                 English: Maximum drawing width.
   * @return 中文：文本高度，单位像素。
   *         English: Text height in pixels.
   */
  int measureBytesHeight(const uint8_t* data, size_t len, int maxWidth) const;

  /**
   * 中文：把 AppState 转为状态栏显示字符串。
   * English: Converts AppState to a status-bar string.
   *
   * @param state 中文：应用状态。
   *              English: Application state.
   * @return 中文：静态字符串指针，不需要释放。
   *         English: Pointer to a static string; no need to free.
   */
  const char* statusText(AppState state) const;

  /**
   * 中文：根据消息角色选择标签和边框颜色。
   * English: Selects label/border color by message role.
   *
   * @param role 中文：消息角色。
   *             English: Message role.
   * @return 中文：RGB565 颜色值。
   *         English: RGB565 color value.
   */
  uint16_t roleColor(ChatRole role) const;

  /**
   * 中文：根据消息角色选择左侧标签文本。
   * English: Selects the left-side label text by message role.
   *
   * @param role 中文：消息角色。
   *             English: Message role.
   * @return 中文：静态标签字符串。
   *         English: Static label string.
   */
  const char* roleLabel(ChatRole role) const;

  /// 中文：Arduino_GFX 数据总线对象，当前使用硬件 SPI。
  /// English: Arduino_GFX data bus object, currently hardware SPI.
  Arduino_DataBus* bus_ = nullptr;

  /// 中文：ST7789 屏幕对象，用于实际绘制。
  /// English: ST7789 display object used for actual drawing.
  Arduino_ST7789* gfx_ = nullptr;

  /// 中文：外置 GB2312 字库 Flash 访问对象。
  /// English: External GB2312 font flash access object.
  Gb2312FontFlash font_;

  /// 中文：字库签名校验是否成功。
  /// English: Whether font signature verification succeeded.
  bool fontOk_ = false;

  /// 中文：固定 UI 框架是否已经绘制，避免每次消息都全屏重画。
  /// English: Whether the fixed UI frame has been drawn to avoid full-screen redraw on every message.
  bool frameDrawn_ = false;
  bool powerFrameDrawn_ = false;
  int lastPowerThinBars_ = -1;
};

}  // namespace WallE
