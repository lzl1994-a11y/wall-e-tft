#pragma once

#include "domain/AppState.h"
#include "domain/ChatSession.h"

namespace WallE {

/**
 * 中文：主显示屏抽象接口；当前实现是 ST7789V 240x240 会话屏。
 * English: Main display abstraction; the current implementation is an ST7789V 240x240 chat display.
 */
class IDisplayPort {
 public:
  /**
   * 中文：虚析构函数，用于接口多态。
   * English: Virtual destructor for polymorphic use through the interface.
   */
  virtual ~IDisplayPort() = default;

  /**
   * 中文：初始化显示硬件和字体资源。
   * English: Initializes display hardware and font resources.
   *
   * @param 无 / None.
   * @return 中文：true 表示初始化成功；false 表示显示或字体硬件初始化失败。
   *         English: true on success; false when display or font hardware initialization fails.
   */
  virtual bool begin() = 0;

  /**
   * 中文：查询外置 GB2312 字库是否通过签名校验。
   * English: Checks whether the external GB2312 font flash passed signature verification.
   *
   * @param 无 / None.
   * @return 中文：true 表示字库可用；false 表示字库不可用或校验失败。
   *         English: true when the font is available; false when missing or verification failed.
   */
  virtual bool fontOk() const = 0;

  /**
   * 中文：刷新状态栏文本。
   * English: Updates the status text area.
   *
   * @param state 中文：当前应用状态。
   *              English: Current application state.
   * @return 无 / None.
   */
  virtual void showStatus(AppState state) = 0;

  /**
   * 中文：完整绘制电量界面；用于启动后或从聊天界面返回电量界面。
   * English: Fully draws the power screen; used after boot or when returning from chat mode.
   *
   * @param percent 中文：电量百分比，范围 0-100。
   *                English: Power percentage in the 0-100 range.
   * @return 无 / None.
   */
  virtual void showPower(uint8_t percent) = 0;

  /**
   * 中文：局部更新电量条块，不重绘标题和太阳图标。
   * English: Partially updates only the power bars, without redrawing the title and sun icon.
   *
   * @param percent 中文：电量百分比，范围 0-100。
   *                English: Power percentage in the 0-100 range.
   * @return 无 / None.
   */
  virtual void updatePower(uint8_t percent) = 0;

  /**
   * 中文：按当前会话内容重绘主屏聊天区域。
   * English: Redraws the main chat area from the current session.
   *
   * @param session 中文：待显示的会话记录，只读取不修改。
   *                English: Chat session to display; read-only.
   * @return 无 / None.
   */
  virtual void render(const ChatSession& session) = 0;
};

}  // namespace WallE
