#pragma once

#include <Arduino.h>
#include <SPI.h>

namespace WallE {

/**
 * 中文：外置 GB2312 点阵字库 Flash 适配器。
 * English: Adapter for the external GB2312 bitmap font flash.
 */
class Gb2312FontFlash {
 public:
  /**
   * 中文：初始化字库 Flash 的 CS 和 SPI，并发送退出低功耗命令。
   * English: Initializes the font flash CS/SPI and sends the release-from-power-down command.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void begin();

  /**
   * 中文：读取 Flash 开头签名并判断是否为预期字库。
   * English: Reads the signature at the beginning of flash and checks whether it is the expected font image.
   *
   * @param 无 / None.
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
   * @return 无 / None.
   */
  void read(uint32_t address, uint8_t* buffer, size_t length);

  /**
   * 中文：读取一个 8x16 ASCII 字符点阵。
   * English: Reads one 8x16 ASCII glyph bitmap.
   *
   * @param ascii 中文：ASCII 字符编码。
   *              English: ASCII character code.
   * @param buffer 中文：接收 16 字节点阵数据的缓冲区。
   *               English: Destination buffer for 16 bitmap bytes.
   * @return 无 / None.
   */
  void readAscii16(uint8_t ascii, uint8_t* buffer);

  /**
   * 中文：读取一个 16x16 GB2312 汉字点阵。
   * English: Reads one 16x16 GB2312 Chinese glyph bitmap.
   *
   * @param high 中文：GB2312/GBK 双字节编码的高字节。
   *             English: High byte of the GB2312/GBK two-byte code.
   * @param low 中文：GB2312/GBK 双字节编码的低字节。
   *            English: Low byte of the GB2312/GBK two-byte code.
   * @param buffer 中文：接收 32 字节点阵数据的缓冲区。
   *               English: Destination buffer for 32 bitmap bytes.
   * @return 无 / None.
   */
  void readWord16(uint8_t high, uint8_t low, uint8_t* buffer);

 private:
  /**
   * 中文：选中字库 Flash 并开始 SPI 事务。
   * English: Selects the font flash and begins an SPI transaction.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void select();

  /**
   * 中文：结束 SPI 事务并取消选中字库 Flash。
   * English: Ends the SPI transaction and deselects the font flash.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  void deselect();
};

}  // namespace WallE
