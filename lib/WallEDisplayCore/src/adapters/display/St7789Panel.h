#pragma once

#include <Arduino_GFX_Library.h>
#include <stdint.h>

namespace WallE {

/**
 * 中文：可复用 ST7789 面板驱动，只负责创建总线、初始化控制器并暴露 Arduino_GFX 绘图对象。
 * English: Reusable ST7789 panel driver that only creates the bus, initializes the controller, and exposes an Arduino_GFX drawing object.
 */
class St7789Panel {
 public:
  /**
   * 中文：ST7789 面板硬件和控制器参数。
   * English: Hardware and controller parameters for an ST7789 panel.
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

    /// 中文：SPI MISO 引脚；不需要读屏或屏幕字库时可设为 GFX_NOT_DEFINED。
    /// English: SPI MISO pin; use GFX_NOT_DEFINED when readback or screen font flash is not needed.
    int misoPin = GFX_NOT_DEFINED;

    /// 中文：TFT 复位引脚。
    /// English: TFT reset pin.
    int rstPin = GFX_NOT_DEFINED;

    /// 中文：屏幕逻辑宽度，单位像素。
    /// English: Logical panel width in pixels.
    int16_t width = 240;

    /// 中文：屏幕逻辑高度，单位像素。
    /// English: Logical panel height in pixels.
    int16_t height = 240;

    /// 中文：屏幕旋转方向，直接传给 Arduino_ST7789。
    /// English: Panel rotation passed directly to Arduino_ST7789.
    uint8_t rotation = 0;

    /// 中文：是否按 IPS 面板初始化。
    /// English: Whether to initialize as an IPS panel.
    bool ips = true;

    /// 中文：第一组列偏移，适配不同 ST7789 模组。
    /// English: First column offset for different ST7789 modules.
    int16_t colOffset1 = 0;

    /// 中文：第一组行偏移，适配不同 ST7789 模组。
    /// English: First row offset for different ST7789 modules.
    int16_t rowOffset1 = 0;

    /// 中文：第二组列偏移，适配不同 ST7789 模组。
    /// English: Second column offset for different ST7789 modules.
    int16_t colOffset2 = 0;

    /// 中文：第二组行偏移，当前 240x240 ST7789V 模组需要保留 80。
    /// English: Second row offset; the current 240x240 ST7789V module needs 80.
    int16_t rowOffset2 = 80;

    /// 中文：TFT SPI 通信频率，单位 Hz。
    /// English: TFT SPI frequency in Hz.
    uint32_t spiHz = 40000000;
  };

  /**
   * 中文：构造面板驱动，仅保存配置，不访问硬件。
   * English: Constructs the panel driver and stores config without touching hardware.
   *
   * @param config 中文：ST7789 面板配置。
   *               English: ST7789 panel config.
   */
  explicit St7789Panel(const Config& config);

  /**
   * 中文：释放由面板驱动创建的 Arduino_GFX 对象。
   * English: Releases Arduino_GFX objects created by the panel driver.
   */
  ~St7789Panel();

  /**
   * 中文：创建 Arduino_GFX 硬件 SPI 总线和 ST7789 控制器，并启动屏幕。
   * English: Creates the Arduino_GFX hardware SPI bus and ST7789 controller, then starts the panel.
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

  /// 中文：Arduino_GFX 硬件 SPI 数据总线对象，由本驱动创建和持有。
  /// English: Arduino_GFX hardware SPI data bus created and owned by this driver.
  Arduino_HWSPI* bus_ = nullptr;

  /// 中文：Arduino_GFX ST7789 控制器对象，由本驱动创建和持有。
  /// English: Arduino_GFX ST7789 controller object created and owned by this driver.
  Arduino_ST7789* gfx_ = nullptr;

  /// 中文：屏幕是否已成功启动。
  /// English: Whether the panel has started successfully.
  bool ready_ = false;
};

}  // namespace WallE
