#pragma once

#include <AnimatedGIF.h>
#include <Arduino_GFX_Library.h>
#include <stddef.h>
#include <stdint.h>

namespace WallE {

/**
 * 中文：通用非阻塞 GIF 播放器，负责 AnimatedGIF 解码和逐行推送到 Arduino_GFX 屏幕。
 * English: Generic non-blocking GIF player that decodes with AnimatedGIF and pushes each line to an Arduino_GFX display.
 */
class GifPlayer {
 public:
  /**
   * 中文：播放或绘图前的总线准备回调，通常用于拉高其它 SPI 设备片选。
   * English: Callback used before playback or drawing, usually to deselect other SPI devices.
   */
  using PrepareBusCallback = void (*)();

  /**
   * 中文：GIF 播放参数。
   * English: GIF playback parameters.
   */
  struct Config {
    /// 中文：最大绘制宽度，用于裁剪 GIF 行，避免越过屏幕边界。
    /// English: Maximum draw width used to clip GIF rows so they do not exceed the display boundary.
    int16_t maxWidth = 240;

    /// 中文：播放前是否先清屏。
    /// English: Whether to clear the display before playback.
    bool clearBeforePlay = true;

    /// 中文：播放前清屏使用的 RGB565 颜色。
    /// English: RGB565 color used when clearing before playback.
    uint16_t clearColor = 0x0000;
  };

  /**
   * 中文：构造 GIF 播放器，仅保存播放参数。
   * English: Constructs the GIF player and stores playback config.
   *
   * @param config 中文：播放参数。
   *               English: Playback config.
   */
  explicit GifPlayer(const Config& config);

  /**
   * 中文：设置绘图目标屏幕；播放器不拥有该对象。
   * English: Sets the target display; the player does not own the object.
   *
   * @param gfx 中文：Arduino_GFX 屏幕对象指针。
   *            English: Arduino_GFX display pointer.
   */
  void setDisplay(Arduino_GFX* gfx) { gfx_ = gfx; }

  /**
   * 中文：设置总线准备回调；播放器不直接依赖项目的 SPI 片选实现。
   * English: Sets the bus preparation callback so the player does not depend on project-specific SPI CS code.
   *
   * @param callback 中文：无参数无返回值函数指针；可为空。
   *                 English: Function pointer with no arguments/return value; nullable.
   */
  void setPrepareBusCallback(PrepareBusCallback callback) {
    prepareBusCallback_ = callback;
  }

  /**
   * 中文：初始化 GIF 解码器像素格式。
   * English: Initializes the GIF decoder pixel format.
   */
  void begin();

  /**
   * 中文：启动一段 GIF 的非阻塞播放；函数立即返回，后续由 update() 推进每一帧。
   * English: Starts non-blocking playback of GIF data and returns immediately; later frames are advanced by update().
   *
   * @param data 中文：GIF 原始字节数据，通常位于 PROGMEM。
   *             English: Raw GIF bytes, usually stored in PROGMEM.
   * @param size 中文：GIF 数据长度，单位字节。
   *             English: GIF data length in bytes.
   * @return 中文：成功打开并进入播放状态返回 true；参数无效或打开失败返回 false。
   *         English: true when the GIF opens and enters playback state; false on invalid parameters or open failure.
   */
  bool play(const uint8_t* data, size_t size);

  /**
   * 中文：推进 GIF 播放状态机；应在主循环或独立任务中周期性调用。
   * English: Advances the GIF playback state machine; call periodically from the main loop or a dedicated task.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void update();

  /**
   * 中文：停止当前播放并释放 GIF 解码器打开的资源。
   * English: Stops current playback and releases resources opened by the GIF decoder.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void stop();

  /**
   * 中文：返回 GIF 是否仍处于播放或等待最后一帧停留的状态。
   * English: Returns whether the GIF is still playing or waiting for the final frame hold time.
   *
   * @return 中文：正在播放返回 true，否则返回 false。
   *         English: true when playback is active; false otherwise.
   */
  bool isPlaying() const { return playing_; }

 private:
  /**
   * 中文：AnimatedGIF 静态行回调，会转发到当前正在播放的 GifPlayer 实例。
   * English: Static AnimatedGIF line callback that forwards to the currently active GifPlayer instance.
   */
  static void gifDrawCallback(GIFDRAW* pDraw);

  /**
   * 中文：把 AnimatedGIF 解码出的单行索引色像素转换为 RGB565 并写入屏幕。
   * English: Converts one decoded indexed-color GIF line to RGB565 and writes it to the display.
   */
  void drawLine(GIFDRAW* pDraw);

  /**
   * 中文：在访问屏幕前执行可选的总线准备动作。
   * English: Runs the optional bus preparation hook before touching display hardware.
   */
  void prepareBus() const;

  /// 中文：播放参数。
  /// English: Playback config.
  Config config_;

  /// 中文：目标屏幕对象，不由播放器释放。
  /// English: Target display object, not owned by the player.
  Arduino_GFX* gfx_ = nullptr;

  /// 中文：AnimatedGIF 解码器实例。
  /// English: AnimatedGIF decoder instance.
  AnimatedGIF gif_;

  /// 中文：可选总线准备回调，不由播放器释放。
  /// English: Optional bus preparation callback, not owned by the player.
  PrepareBusCallback prepareBusCallback_ = nullptr;

  /// 中文：GIF 文件当前是否处于已打开状态。
  /// English: Whether the GIF file is currently open.
  bool opened_ = false;

  /// 中文：GIF 是否处于活动播放状态。
  /// English: Whether GIF playback is currently active.
  bool playing_ = false;

  /// 中文：最后一帧已绘制，正在等待其停留时间结束。
  /// English: The last frame has been drawn and the player is waiting for its hold time to expire.
  bool waitingFinalDelay_ = false;

  /// 中文：下一帧允许解码的时间点，单位毫秒。
  /// English: Timestamp in milliseconds when the next frame may be decoded.
  uint32_t nextFrameAtMs_ = 0;
};

}  // namespace WallE
