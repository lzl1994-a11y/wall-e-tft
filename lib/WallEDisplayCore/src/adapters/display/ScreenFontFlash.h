#pragma once

#include "ports/IBitmapFontProvider.h"
#include <Arduino.h>
#include <SPI.h>

namespace WallE {

/**
 * 中文：屏幕板载/外接 SPI 点阵字库 Flash 适配器。
 * English: Adapter for an onboard/external SPI bitmap font flash.
 */
class ScreenFontFlash : public IBitmapFontProvider {
 public:
  /**
   * 中文：字库 Flash 的硬件连接与字模布局配置。
   * English: Hardware wiring and glyph layout configuration for the font flash.
   */
  struct Config {
    /// 中文：字库 Flash 片选引脚；小于 0 表示未连接。
    /// English: Font flash chip-select pin; negative means not connected.
    int csPin = -1;

    /// 中文：SPI SCK 引脚。
    /// English: SPI SCK pin.
    int sckPin = -1;

    /// 中文：SPI MISO 引脚；读取字库 Flash 时必须有效。
    /// English: SPI MISO pin; required when reading the font flash.
    int misoPin = -1;

    /// 中文：SPI MOSI 引脚。
    /// English: SPI MOSI pin.
    int mosiPin = -1;

    /// 中文：字库 Flash SPI 频率，单位 Hz。
    /// English: SPI frequency for the font flash, in Hz.
    uint32_t spiHz = 2000000;

    /// 中文：8x16 单字节字模区起始地址。
    /// English: Start address of the 8x16 single-byte glyph region.
    uint32_t ascii8x16Base = 0x1D00;

    /// 中文：16x16 双字节字模区起始地址。
    /// English: Start address of the 16x16 double-byte glyph region.
    uint32_t doubleByte16x16Base = 0x71300;

    /// 中文：双字节字库的首个有效区位码字节，默认兼容 0xA1 起始的 94x94 编码表。
    /// English: First valid byte for double-byte indexing; default matches 0xA1-based 94x94 code pages.
    uint8_t doubleByteFirst = 0xA1;

    /// 中文：双字节字库每个区的字符数量，默认 94。
    /// English: Number of glyphs in each double-byte area; default is 94.
    uint8_t doubleByteColumns = 94;
  };

  /**
   * 中文：构造字库 Flash 适配器，只保存配置，不立即访问硬件。
   * English: Constructs the font flash adapter and stores configuration without touching hardware.
   *
   * @param config 中文：硬件引脚和字模布局配置。
   *               English: Hardware pin and glyph layout configuration.
   */
  explicit ScreenFontFlash(const Config& config);

  /**
   * 中文：初始化字库 Flash 的 CS、SPI，并发送唤醒命令。
   * English: Initializes the font flash CS/SPI and sends the wake command.
   *
   * @return 中文：初始化成功返回 true；配置缺失返回 false。
   *         English: true on successful initialization; false when required config is missing.
   */
  bool begin() override;

  /**
   * 中文：返回签名校验后的字库可用状态。
   * English: Returns the font availability after signature verification.
   *
   * @return 中文：字库可用返回 true，否则返回 false。
   *         English: true when the font is usable; false otherwise.
   */
  bool available() const override { return available_; }

  /**
   * 中文：读取一个 8x16 单字节字模。
   * English: Reads one 8x16 single-byte glyph bitmap.
   *
   * @param code 中文：单字节编码。
   *             English: Single-byte code.
   * @param buffer 中文：接收 16 字节点阵数据的缓冲区。
   *               English: Destination buffer for 16 bitmap bytes.
   * @param length 中文：缓冲区长度，必须至少为 16。
   *               English: Buffer length, must be at least 16.
   * @return 中文：读取成功返回 true，否则返回 false。
   *         English: true on success; false otherwise.
   */
  bool readAscii8x16(uint8_t code, uint8_t* buffer, size_t length) override;

  /**
   * 中文：读取一个 16x16 双字节字模。
   * English: Reads one 16x16 double-byte glyph bitmap.
   *
   * @param high 中文：双字节编码高字节。
   *             English: High byte of the double-byte code.
   * @param low 中文：双字节编码低字节。
   *            English: Low byte of the double-byte code.
   * @param buffer 中文：接收 32 字节点阵数据的缓冲区。
   *               English: Destination buffer for 32 bitmap bytes.
   * @param length 中文：缓冲区长度，必须至少为 32。
   *               English: Buffer length, must be at least 32.
   * @return 中文：读取成功返回 true，否则返回 false。
   *         English: true on success; false otherwise.
   */
  bool readDoubleByte16x16(uint8_t high, uint8_t low, uint8_t* buffer,
                           size_t length) override;

 private:
  /**
   * 中文：读取 Flash 签名并判断是否为预期字库镜像。
   * English: Reads the flash signature and checks whether it is the expected font image.
   *
   * @return 中文：签名匹配返回 true，否则返回 false。
   *         English: true when the signature matches; false otherwise.
   */
  bool checkSignature();

  /**
   * 中文：从 Flash 指定地址读取任意字节数据。
   * English: Reads arbitrary bytes from a specified flash address.
   *
   * @param address 中文：Flash 内部 24 位起始地址。
   *                English: 24-bit start address inside the flash.
   * @param buffer 中文：接收缓冲区指针。
   *               English: Destination buffer pointer.
   * @param length 中文：要读取的字节数。
   *               English: Number of bytes to read.
   * @return 中文：读取成功返回 true，否则返回 false。
   *         English: true on success; false otherwise.
   */
  bool read(uint32_t address, uint8_t* buffer, size_t length);

  /**
   * 中文：选中字库 Flash 并开始 SPI 事务。
   * English: Selects the font flash and begins an SPI transaction.
   */
  void select();

  /**
   * 中文：结束 SPI 事务并取消选中字库 Flash。
   * English: Ends the SPI transaction and deselects the font flash.
   */
  void deselect();

  /// 中文：硬件与布局配置。
  /// English: Hardware and layout configuration.
  Config config_;

  /// 中文：初始化是否已完成。
  /// English: Whether initialization has completed.
  bool initialized_ = false;

  /// 中文：签名校验后的可用状态。
  /// English: Availability after signature verification.
  bool available_ = false;
};

}  // namespace WallE
