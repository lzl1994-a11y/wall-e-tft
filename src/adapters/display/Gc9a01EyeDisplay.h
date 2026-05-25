#pragma once

#include <AnimatedGIF.h>
#include <Arduino_GFX_Library.h>
#include "ports/IEyeDisplayPort.h"

namespace WallE {

/**
 * 中文：GC9A01 圆形眼睛屏显示适配器，负责初始化屏幕并播放内置 GIF。
 * English: GC9A01 round eye display adapter that initializes the panel and plays the embedded GIF.
 */
class Gc9a01EyeDisplay : public IEyeDisplayPort {
 public:
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

 private:
  /// 中文：Arduino_GFX 的 ESP32 SPI 总线对象，由本类创建并复用。
  /// English: Arduino_GFX ESP32 SPI bus object created and reused by this class.
  Arduino_ESP32SPI* bus_ = nullptr;

  /// 中文：GC9A01 屏幕对象，用于真正绘制像素。
  /// English: GC9A01 display object used to draw pixels.
  Arduino_GC9A01* gfx_ = nullptr;

  /// 中文：AnimatedGIF 解码器实例，用于从 PROGMEM GIF 数据解码帧。
  /// English: AnimatedGIF decoder instance used to decode frames from PROGMEM GIF data.
  AnimatedGIF gif_;

  /**
   * 中文：播放内置 GIF 的内部实现；该函数是阻塞式，会播放完整个 GIF 后返回。
   * English: Internal implementation for playing the embedded GIF; this is blocking and returns after the full GIF finishes.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void playGif();

  /**
   * 中文：AnimatedGIF 行绘制回调，把解码出的调色板像素写到 GC9A01。
   * English: AnimatedGIF line-draw callback that writes decoded palette pixels to the GC9A01 display.
   *
   * @param pDraw 中文：AnimatedGIF 提供的当前行绘制信息。
   *              English: Current line drawing information provided by AnimatedGIF.
   * @return 无 / None.
   */
  static void gifDrawCallback(GIFDRAW* pDraw);
};

}  // namespace WallE
