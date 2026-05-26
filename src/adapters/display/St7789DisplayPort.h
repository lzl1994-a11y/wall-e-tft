#pragma once

#include "adapters/display/ScreenFontFlash.h"
#include "adapters/display/St7789Panel.h"
#include "adapters/display/TextRenderer.h"
#include "ports/IDisplayPort.h"

namespace WallE {

/**
 * 中文：胸前 ST7789 主屏显示适配器，负责绘制启动页、状态栏和聊天会话。
 * English: Chest ST7789 main display adapter that draws boot page, status bar, and chat session.
 */
class St7789DisplayPort : public IDisplayPort {
 public:
  /**
   * 中文：构造主屏适配器，并把项目配置转换为可复用的面板和字库配置。
   * English: Constructs the main display adapter and converts project settings into reusable panel and font config.
   */
  St7789DisplayPort();

  /**
   * 中文：初始化 ST7789 屏幕和当前选择的文字字模资源。
   * English: Initializes the ST7789 display and the selected text font resource.
   *
   * @param 无 / None.
   * @return 中文：true 表示屏幕初始化成功；false 表示屏幕初始化失败。
   *         English: true when display initialization succeeds; false on display initialization failure.
   */
  bool begin() override;

  /**
   * 中文：返回当前文字渲染资源是否可用。
   * English: Returns whether the selected text rendering resource is usable.
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

  /// 中文：ST7789 面板驱动，负责屏幕硬件初始化和 Arduino_GFX 对象生命周期。
  /// English: ST7789 panel driver responsible for hardware initialization and Arduino_GFX object lifetime.
  St7789Panel panel_;

  /// 中文：ST7789 通用绘图对象；由 panel_ 持有，这里只缓存指针方便 UI 绘制。
  /// English: Generic ST7789 drawing object owned by panel_; cached here only for UI drawing.
  Arduino_GFX* gfx_ = nullptr;

  /// 中文：屏幕字库 Flash 访问对象；仅配置为 ScreenFontFlash 时启用。
  /// English: Screen font flash access object; enabled only when configured as ScreenFontFlash.
  ScreenFontFlash screenFont_;

  /// 中文：独立文本渲染器，负责字节文本测量、换行和字形绘制。
  /// English: Dedicated text renderer for byte-text measuring, wrapping, and glyph drawing.
  TextRenderer textRenderer_;

  /// 中文：当前文字渲染资源是否可用。
  /// English: Whether the selected text rendering resource is usable.
  bool fontOk_ = false;

  /// 中文：固定 UI 框架是否已经绘制，避免每次消息都全屏重画。
  /// English: Whether the fixed UI frame has been drawn to avoid full-screen redraw on every message.
  bool frameDrawn_ = false;
  bool powerFrameDrawn_ = false;
  int lastPowerThinBars_ = -1;
};

}  // namespace WallE
