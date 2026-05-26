#pragma once

#include <stddef.h>
#include <stdint.h>

namespace WallE {

/**
 * 中文：点阵字模数据源接口；屏幕驱动只依赖“取字模”能力，不关心字库芯片型号或编码名称。
 * English: Bitmap font source interface; display drivers depend only on glyph access, not on a specific font chip or encoding name.
 */
class IBitmapFontProvider {
 public:
  /**
   * 中文：虚析构函数，用于通过接口指针安全释放派生对象。
   * English: Virtual destructor for safe polymorphic use.
   */
  virtual ~IBitmapFontProvider() = default;

  /**
   * 中文：初始化字模数据源，例如配置 CS 引脚、SPI 总线或外部 Flash 唤醒命令。
   * English: Initializes the glyph source, such as CS pin, SPI bus, or external flash wake command.
   *
   * @return 中文：初始化流程成功返回 true；硬件配置无效或通信失败返回 false。
   *         English: true on successful initialization; false on invalid hardware config or communication failure.
   */
  virtual bool begin() = 0;

  /**
   * 中文：查询字模数据源当前是否可用于渲染。
   * English: Checks whether the glyph source is currently usable for rendering.
   *
   * @return 中文：可用返回 true；未初始化、签名校验失败或未接入返回 false。
   *         English: true when usable; false when uninitialized, signature check failed, or absent.
   */
  virtual bool available() const = 0;

  /**
   * 中文：读取一个 8x16 单字节字模。
   * English: Reads one 8x16 single-byte glyph bitmap.
   *
   * @param code 中文：单字节字符编码。
   *             English: Single-byte character code.
   * @param buffer 中文：接收 16 字节点阵数据的缓冲区。
   *               English: Destination buffer for 16 bitmap bytes.
   * @param length 中文：缓冲区长度，必须至少为 16。
   *               English: Buffer length, must be at least 16.
   * @return 中文：读取成功返回 true；参数无效或数据源不可用返回 false。
   *         English: true on success; false on invalid parameters or unavailable source.
   */
  virtual bool readAscii8x16(uint8_t code, uint8_t* buffer, size_t length) = 0;

  /**
   * 中文：读取一个 16x16 双字节字模。
   * English: Reads one 16x16 double-byte glyph bitmap.
   *
   * @param high 中文：双字节编码的高字节。
   *             English: High byte of the double-byte code.
   * @param low 中文：双字节编码的低字节。
   *            English: Low byte of the double-byte code.
   * @param buffer 中文：接收 32 字节点阵数据的缓冲区。
   *               English: Destination buffer for 32 bitmap bytes.
   * @param length 中文：缓冲区长度，必须至少为 32。
   *               English: Buffer length, must be at least 32.
   * @return 中文：读取成功返回 true；参数无效或数据源不可用返回 false。
   *         English: true on success; false on invalid parameters or unavailable source.
   */
  virtual bool readDoubleByte16x16(uint8_t high, uint8_t low, uint8_t* buffer,
                                   size_t length) = 0;
};

}  // namespace WallE
