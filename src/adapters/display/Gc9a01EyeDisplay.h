#pragma once

#include "adapters/display/Gc9a01Panel.h"
#include "adapters/display/GifPlayer.h"
#include "ports/IEyeDisplayPort.h"

namespace WallE {

/**
 * 中文：GC9A01 圆形眼睛屏显示适配器，负责初始化屏幕并播放内置 GIF。
 * English: GC9A01 round eye display adapter that initializes the panel and plays the embedded GIF.
 */
class Gc9a01EyeDisplay : public IEyeDisplayPort {
 public:
  /**
   * 中文：构造眼睛显示适配器，并把项目配置转换为可复用的面板和 GIF 播放配置。
   * English: Constructs the eye display adapter and converts project settings into reusable panel and GIF playback config.
   */
  Gc9a01EyeDisplay();

  /**
   * 中文：初始化眼睛屏 SPI、GC9A01 控制器和 GIF 解码器。
   * English: Initializes the eye display SPI bus, GC9A01 controller, and GIF decoder.
   *
   * @param 无 / None.
   * @return 中文：true 表示初始化成功；false 表示内存分配或屏幕初始化失败。
   *         English: true on success; false on allocation or display initialization failure.
   */
  bool begin();

  /**
   * 中文：播放 zoom 眼睛动作，目前直接播放 eyeimg.h 中的 GIF。
   * English: Plays the zoom eye action, currently by playing the GIF stored in eyeimg.h.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void playZoom() override;

  /**
   * 中文：推进当前眼睛动画播放状态；用于把 GIF 播放改成非阻塞模式。
   * English: Advances the current eye animation playback state, enabling non-blocking GIF playback.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void update() override;

 private:
  /// 中文：GC9A01 面板驱动，负责屏幕硬件初始化和 Arduino_GFX 对象生命周期。
  /// English: GC9A01 panel driver responsible for hardware initialization and Arduino_GFX object lifetime.
  Gc9a01Panel panel_;

  /// 中文：GIF 播放器，负责 AnimatedGIF 解码和逐行推送。
  /// English: GIF player responsible for AnimatedGIF decoding and line-by-line drawing.
  GifPlayer gifPlayer_;

  /// 中文：眼睛屏是否已成功初始化。
  /// English: Whether the eye display has initialized successfully.
  bool ready_ = false;
};

}  // namespace WallE
