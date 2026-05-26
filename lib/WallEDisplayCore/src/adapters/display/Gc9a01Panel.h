#pragma once

#include <Arduino_GFX_Library.h>
#include <stdint.h>

namespace WallE {

/**
 * 中文：可复用 GC9A01 圆屏面板驱动，只负责 SPI 总线、控制器对象和屏幕启动。
 * English: Reusable GC9A01 round panel driver that only handles the SPI bus, controller object, and panel startup.
 */
class Gc9a01Panel {
 public:
  /**
   * 中文：GC9A01 面板硬件和控制器参数。
   * English: Hardware and controller parameters for a GC9A01 panel.
   */
  struct Config {
    /// 中文：D/C 数据命令选择引脚。
    /// English: D/C data-command pin.
    int dcPin = GFX_NOT_DEFINED;

    /// 中文：TFT 片选引脚。
    /// English: TFT chip-select pin.
    int csPin = GFX_NOT_DEFINED;

    /// 中文：SPI SCK 引脚。
    /// English: SPI SCK pin.
    int sckPin = GFX_NOT_DEFINED;

    /// 中文：SPI MOSI 引脚。
    /// English: SPI MOSI pin.
    int mosiPin = GFX_NOT_DEFINED;

    /// 中文：SPI MISO 引脚；GC9A01 只写显示通常不需要。
    /// English: SPI MISO pin; usually not needed for write-only GC9A01 display.
    int misoPin = GFX_NOT_DEFINED;

    /// 中文：TFT 复位引脚。
    /// English: TFT reset pin.
    int rstPin = GFX_NOT_DEFINED;

    /// 中文：ESP32 SPI Host 编号，当前眼睛屏使用 HSPI/软件独立总线配置。
    /// English: ESP32 SPI host number; the current eye display uses HSPI/independent software-style bus config.
    int8_t spiHost = HSPI;

    /// 中文：Arduino_GFX SPI 共享接口标志，保持与原眼睛屏初始化参数一致。
    /// English: Arduino_GFX SPI shared-interface flag, kept consistent with the previous eye display initialization.
    bool sharedInterface = false;

    /// 中文：屏幕逻辑宽度，单位像素。
    /// English: Logical panel width in pixels.
    int16_t width = 240;

    /// 中文：屏幕逻辑高度，单位像素。
    /// English: Logical panel height in pixels.
    int16_t height = 240;

    /// 中文：屏幕旋转方向，直接传给 Arduino_GC9A01。
    /// English: Panel rotation passed directly to Arduino_GC9A01.
    uint8_t rotation = 0;

    /// 中文：是否按 IPS 面板初始化。
    /// English: Whether to initialize as an IPS panel.
    bool ips = true;

    /// 中文：TFT SPI 通信频率，单位 Hz。
    /// English: TFT SPI frequency in Hz.
    uint32_t spiHz = 40000000;
  };

  /**
   * 中文：构造 GC9A01 面板驱动，仅保存配置，不访问硬件。
   * English: Constructs the GC9A01 panel driver and stores config without touching hardware.
   *
   * @param config 中文：GC9A01 面板配置。
   *               English: GC9A01 panel config.
   */
  explicit Gc9a01Panel(const Config& config);

  /**
   * 中文：释放由面板驱动创建的 Arduino_GFX 对象。
   * English: Releases Arduino_GFX objects created by the panel driver.
   */
  ~Gc9a01Panel();

  /**
   * 中文：创建 Arduino_GFX ESP32 SPI 总线和 GC9A01 控制器，并启动屏幕。
   * English: Creates the Arduino_GFX ESP32 SPI bus and GC9A01 controller, then starts the panel.
   *
   * @return 中文：初始化成功返回 true；对象创建或 begin() 失败返回 false。
   *         English: true on success; false if object creation or begin() fails.
   */
  bool begin();

  /**
   * 中文：返回通用 Arduino_GFX 绘图对象，调用方不拥有该对象。
   * English: Returns the generic Arduino_GFX drawing object; caller does not own it.
   *
   * @return 中文：屏幕对象指针；未初始化时可能为 nullptr。
   *         English: Display object pointer; may be nullptr before initialization.
   */
  Arduino_GFX* gfx() const { return gfx_; }

  /**
   * 中文：返回面板是否已经成功初始化。
   * English: Returns whether the panel has been initialized successfully.
   *
   * @return 中文：已初始化返回 true，否则返回 false。
   *         English: true when initialized; false otherwise.
   */
  bool ready() const { return ready_; }

 private:
  /// 中文：面板硬件和控制器配置。
  /// English: Panel hardware and controller config.
  Config config_;

  /// 中文：Arduino_GFX ESP32 SPI 总线对象，由本驱动创建和持有。
  /// English: Arduino_GFX ESP32 SPI bus object created and owned by this driver.
  Arduino_ESP32SPI* bus_ = nullptr;

  /// 中文：Arduino_GFX GC9A01 控制器对象，由本驱动创建和持有。
  /// English: Arduino_GFX GC9A01 controller object created and owned by this driver.
  Arduino_GC9A01* gfx_ = nullptr;

  /// 中文：屏幕是否已成功启动。
  /// English: Whether the panel has started successfully.
  bool ready_ = false;
};

}  // namespace WallE
