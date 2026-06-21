#pragma once

#include <Arduino.h>

/**
 * 中文：项目硬件与运行参数配置命名空间。
 * English: Namespace for project hardware and runtime configuration values.
 */
namespace WallEConfig {

/// 中文：未使用的 GPIO 标记；用于“没有 MISO / 没有 CSF”的屏幕配置。
/// English: Marker for unused GPIOs; useful for panels without MISO / CSF.
constexpr int kNoPin = -1;

/// 中文：USB/串口通信波特率，必须和上位机发送端保持一致。
/// English: USB/Serial baud rate; must match the host-side sender.
constexpr uint32_t kSerialBaud = 115200;

/// 中文：串口无换行时的空闲提交时间；超过该时间会把当前缓冲当成一条消息。
/// English: Idle timeout for committing Serial data without a newline.
constexpr uint32_t kSerialIdleMs = 250;

/// 中文：进入聊天界面后，长时间没有 you:/ai: 消息时自动返回电量界面的时间。
/// English: Idle timeout for returning from chat screen to power screen.
constexpr uint32_t kChatIdleReturnMs = 30000;

/// 中文：单条串口输入最大字节数，超出部分会被丢弃。
/// English: Maximum bytes per Serial input packet; bytes beyond this limit are dropped.
constexpr size_t kInputMaxBytes = 512;

/// 中文：主屏最多保留的会话消息条数，满了会删除最旧消息。
/// English: Maximum chat messages kept for the main screen.
constexpr size_t kChatMaxMessages = 8;

/// 中文：是否启用 GC9A01 眼睛屏初始化和动作播放。
/// English: Enables initialization and action playback for the GC9A01 eye display.
constexpr bool kEnableEyeDisplay = true;

/// 中文：是否在开机时显示 ST7789 彩条自检画面。
/// English: Enables the ST7789 color-bar self-test screen at boot.
constexpr bool kMainTftSelfTestOnBoot = true;

/**
 * 中文：主屏文字字模来源。
 * English: Font source used by the main display text renderer.
 */
enum class TextFontSource : uint8_t {
  /// 中文：不使用屏幕字库，改用 Arduino_GFX 内置 ASCII 字体；不需要 MISO 和 CSF。
  /// English: Use Arduino_GFX built-in ASCII font; MISO and CSF are not required.
  ArduinoGfx,

  /// 中文：使用 ST7789V 模组板载/配套 SPI 字库 Flash；需要配置 MISO 和 CSF。
  /// English: Use onboard/paired SPI font flash on the ST7789V module; MISO and CSF are required.
  ScreenFontFlash,
};

/// 中文：当前主屏文字来源；不带字库的屏幕可改为 TextFontSource::ArduinoGfx。
/// English: Current main text source; set to TextFontSource::ArduinoGfx for panels without font flash.
constexpr TextFontSource kMainTextFontSource = TextFontSource::ScreenFontFlash;

/// 中文：代码内部使用的便捷开关，避免未使用字库时初始化 MISO/CSF。
/// English: Internal convenience flag to avoid initializing MISO/CSF when the screen font is unused.
constexpr bool kUseScreenFontFlash = kMainTextFontSource == TextFontSource::ScreenFontFlash;

// 中文：下面保留旧接线配置，方便以后对照；当前不参与编译。
// English: Legacy wiring is kept below for reference only; it is not compiled.
// constexpr int kTftSck = 18;
// constexpr int kTftMosi = 23;
// constexpr int kTftMiso = 19;
// constexpr int kTftDc = 17;
// constexpr int kTftRst = 16;
// constexpr int kTftCs = 21;
// constexpr int kScreenFontCs = 15;

// 中文：胸前 ST7789 主屏 SPI 引脚；MISO 仅在使用屏幕字库 Flash 时需要。
// English: SPI pins for the chest ST7789 main display; MISO is required only when using the screen font flash.
constexpr int kTftSck = 18;
constexpr int kTftMosi = 17;
constexpr int kTftMiso = 16;
constexpr int kTftDc = 13;
constexpr int kTftRst = 15;
constexpr int kTftCs = 14;

// 中文：ST7789V 模组板载/配套屏幕字库 Flash 片选；仅 kUseScreenFontFlash 为 true 时使用。
// English: CS pin for the ST7789V onboard/paired screen font flash; used only when kUseScreenFontFlash is true.
constexpr int kScreenFontCs = 11;

// 中文：瓦力眼睛 GC9A01 圆屏 SPI 引脚；当前为独立眼睛屏总线配置。
// English: SPI pins for the WALL-E eye GC9A01 round display; currently configured as an independent eye-display bus.
constexpr int eysTftSck = 9;
constexpr int eysTftMosi = 8;
constexpr int eysTftDc = 5;
constexpr int eysTftRst = 3;
constexpr int eysTftCs = 4;

// 中文：PCA9685 舵机驱动板 I2C 引脚（可根据实际硬件连线修改）
// English: I2C pins for PCA9685 servo driver (modify according to actual wiring)
constexpr int kPca9685Sda = 1;
constexpr int kPca9685Scl = 2;

/// 中文：主屏和眼睛屏的逻辑宽度，单位像素。
/// English: Logical width of both main and eye displays in pixels.
constexpr int kScreenWidth = 240;

/// 中文：主屏和眼睛屏的逻辑高度，单位像素。
/// English: Logical height of both main and eye displays in pixels.
constexpr int kScreenHeight = 240;

/// 中文：胸前 ST7789 主屏 SPI 频率，单位 Hz。
/// English: SPI frequency for the chest ST7789 main display, in Hz.
constexpr uint32_t kTftSpiHz = 80000000;

/// 中文：眼睛 GC9A01 屏 SPI 频率，单位 Hz。
/// English: SPI frequency for the eye GC9A01 display, in Hz.
constexpr uint32_t keyeTftSpiHz = 40000000;

/// 中文：屏幕字库 Flash SPI 频率，单位 Hz；较低频率通常更稳定。
/// English: SPI frequency for the screen font flash, in Hz; a lower value improves stability.
constexpr uint32_t kScreenFontSpiHz = 2000000;

/// 中文：8x16 单字节字模在屏幕字库 Flash 中的起始地址。
/// English: Start address of 8x16 single-byte glyphs in the screen font flash.
constexpr uint32_t kScreenFontAscii8x16Base = 0x1D00;

/// 中文：16x16 双字节字模在屏幕字库 Flash 中的起始地址。
/// English: Start address of 16x16 double-byte glyphs in the screen font flash.
constexpr uint32_t kScreenFontDoubleByte16x16Base = 0x71300;

/// 中文：双字节字库索引起始字节；默认兼容 0xA1 起始的 94x94 编码表。
/// English: First byte used for double-byte glyph indexing; default matches 0xA1-based 94x94 code pages.
constexpr uint8_t kScreenFontDoubleByteFirst = 0xA1;

/// 中文：双字节字库每个区的字模数量；默认 94。
/// English: Number of glyphs in each double-byte area; default is 94.
constexpr uint8_t kScreenFontDoubleByteColumns = 94;

}  // namespace WallEConfig
