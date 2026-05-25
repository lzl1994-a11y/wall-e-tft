#pragma once

#include <Arduino.h>

/**
 * 中文：项目硬件与运行参数配置命名空间。
 * English: Namespace for project hardware and runtime configuration values.
 */
namespace WallEConfig {

/// 中文：USB/串口通信波特率，必须和上位机发送端保持一致。
/// English: USB/Serial baud rate; must match the host-side sender.
constexpr uint32_t kSerialBaud = 115200;

/// 中文：串口无换行时的空闲提交时间；超过该时间会把当前缓存当成一条消息。
/// English: Idle timeout for committing Serial data without a newline; buffered bytes become one message after this duration.
constexpr uint32_t kSerialIdleMs = 250;
constexpr uint32_t kChatIdleReturnMs = 30000;

/// 中文：单条串口输入最大字节数，超出部分会被丢弃。
/// English: Maximum bytes per Serial input packet; bytes beyond this limit are dropped.
constexpr size_t kInputMaxBytes = 512;

/// 中文：主屏最多保留的会话消息条数，满了会删除最旧消息。
/// English: Maximum chat messages kept for the main screen; oldest messages are removed when full.
constexpr size_t kChatMaxMessages = 8;

/// 中文：是否启用 GC9A01 眼睛屏初始化和动作播放。
/// English: Enables initialization and action playback for the GC9A01 eye display.
constexpr bool kEnableEyeDisplay = true;

/// 中文：是否在开机时显示 ST7789 彩条自检画面。
/// English: Enables the ST7789 color-bar self-test screen at boot.
constexpr bool kMainTftSelfTestOnBoot = true;

// 中文：下面保留旧接线配置，方便以后对照；当前不参与编译。
// English: Legacy wiring is kept below for reference only; it is not compiled.
// constexpr int kTftSck = 18;
// constexpr int kTftMosi = 23;
// constexpr int kTftMiso = 19;
// constexpr int kTftDc = 17;
// constexpr int kTftRst = 16;
// constexpr int kTftCs = 21;
// constexpr int kFontCs = 15;

/// 中文：胸前 ST7789 主屏 SPI 时钟引脚。
/// English: SPI clock pin for the chest ST7789 main display.
constexpr int kTftSck = 18;

/// 中文：胸前 ST7789 主屏 SPI MOSI 引脚。
/// English: SPI MOSI pin for the chest ST7789 main display.
constexpr int kTftMosi = 17;

/// 中文：胸前 ST7789 主屏 SPI MISO 引脚，同时供字库 Flash 读取使用。
/// English: SPI MISO pin for the chest ST7789 bus, also used to read the font flash.
constexpr int kTftMiso = 16;

/// 中文：胸前 ST7789 主屏 D/C 数据命令选择引脚。
/// English: D/C data-command pin for the chest ST7789 main display.
constexpr int kTftDc = 13;

/// 中文：胸前 ST7789 主屏复位引脚。
/// English: Reset pin for the chest ST7789 main display.
constexpr int kTftRst = 15;

/// 中文：胸前 ST7789 主屏片选引脚。
/// English: Chip-select pin for the chest ST7789 main display.
constexpr int kTftCs = 14;

/// 中文：外置 GB2312 字库 Flash 片选引脚。
/// English: Chip-select pin for the external GB2312 font flash.
constexpr int kFontCs = 11;

/// 中文：瓦力眼睛 GC9A01 屏 SPI 时钟引脚。
/// English: SPI clock pin for the WALL-E eye GC9A01 display.
constexpr int eysTftSck = 9;

/// 中文：瓦力眼睛 GC9A01 屏 SPI MOSI 引脚。
/// English: SPI MOSI pin for the WALL-E eye GC9A01 display.
constexpr int eysTftMosi = 8;

/// 中文：瓦力眼睛 GC9A01 屏 D/C 数据命令选择引脚。
/// English: D/C data-command pin for the WALL-E eye GC9A01 display.
constexpr int eysTftDc = 5;

/// 中文：瓦力眼睛 GC9A01 屏复位引脚。
/// English: Reset pin for the WALL-E eye GC9A01 display.
constexpr int eysTftRst = 3;

/// 中文：瓦力眼睛 GC9A01 屏片选引脚。
/// English: Chip-select pin for the WALL-E eye GC9A01 display.
constexpr int eysTftCs = 4;

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

/// 中文：外置字库 Flash SPI 频率，单位 Hz；较低频率更稳。
/// English: SPI frequency for the external font flash, in Hz; a lower value improves stability.
constexpr uint32_t kFontSpiHz = 2000000;

}  // namespace WallEConfig
