#pragma once

#include "ports/IBitmapFontProvider.h"
#include <Arduino_GFX_Library.h>
#include <stddef.h>
#include <stdint.h>

namespace WallE {

/**
 * 中文：文本渲染器，负责原始字节文本的换行、测量和字形绘制。
 * English: Text renderer responsible for wrapping, measuring, and drawing raw byte text.
 */
class TextRenderer {
 public:
  /**
   * 中文：绘图或读取字模前的总线准备回调，通常用于拉高其它 SPI 设备片选。
   * English: Callback used before drawing or reading glyph data, usually to deselect other SPI devices.
   */
  using PrepareBusCallback = void (*)();

  /**
   * 中文：文本渲染参数；把编码判断和字形尺寸从项目配置中解耦出来。
   * English: Text rendering parameters; keeps encoding detection and glyph sizes independent from project config.
   */
  struct Config {
    /// 中文：双字节字模高字节的最小有效值。
    /// English: Minimum valid high byte for double-byte glyphs.
    uint8_t doubleByteFirst = 0xA1;

    /// 中文：双字节字模低字节的最小有效值。
    /// English: Minimum valid low byte for double-byte glyphs.
    uint8_t doubleByteSecondFirst = 0xA1;

    /// 中文：ASCII 字符绘制宽度，匹配 Arduino_GFX 内置 5x7 字体放在 8 像素网格内。
    /// English: ASCII cell width, matching Arduino_GFX built-in 5x7 font in an 8-pixel cell.
    int asciiWidth = 8;

    /// 中文：双字节字模绘制宽度。
    /// English: Draw width for double-byte glyphs.
    int doubleByteWidth = 16;

    /// 中文：统一行高。
    /// English: Common line height.
    int lineHeight = 16;

    /// 中文：ASCII 基线下移量，让英文数字在 16 像素行高中垂直居中。
    /// English: ASCII y offset used to center English letters/numbers in a 16-pixel row.
    int asciiYOffset = 4;
  };

  /**
   * 中文：设置绘图目标屏幕；渲染器不拥有该对象。
   * English: Sets the target display; the renderer does not own the object.
   *
   * @param gfx 中文：Arduino_GFX 屏幕对象指针。
   *            English: Arduino_GFX display pointer.
   */
  void setDisplay(Arduino_GFX* gfx) { gfx_ = gfx; }

  /**
   * 中文：设置文本渲染参数，例如双字节识别阈值和字形尺寸。
   * English: Sets text rendering parameters such as double-byte thresholds and glyph sizes.
   *
   * @param config 中文：新的渲染配置，会被复制保存。
   *               English: New render config, copied into the renderer.
   */
  void setConfig(const Config& config) { config_ = config; }

  /**
   * 中文：设置可选的点阵字模数据源；为空时中文会按不可显示字节处理。
   * English: Sets the optional bitmap font source; null treats Chinese bytes as unsupported.
   *
   * @param fontProvider 中文：点阵字模数据源指针，不转移所有权。
   *                     English: Bitmap font provider pointer; ownership is not transferred.
   */
  void setFontProvider(IBitmapFontProvider* fontProvider) { fontProvider_ = fontProvider; }

  /**
   * 中文：设置总线准备回调；渲染器不直接依赖具体项目的 SPI 片选实现。
   * English: Sets the bus preparation callback so the renderer does not depend on project-specific SPI CS code.
   *
   * @param callback 中文：无参数无返回值函数指针；可为空。
   *                 English: Function pointer with no arguments/return value; nullable.
   */
  void setPrepareBusCallback(PrepareBusCallback callback) {
    prepareBusCallback_ = callback;
  }

  /**
   * 中文：按字节绘制混合文本；ASCII 使用 Arduino_GFX 内置字体，双字节文本使用字模数据源。
   * English: Draws mixed byte text; ASCII uses the Arduino_GFX built-in font and double-byte text uses the glyph provider.
   *
   * @param x 中文：文本区域左上角 x 坐标。
   *          English: Top-left x coordinate of the text area.
   * @param y 中文：文本区域左上角 y 坐标。
   *          English: Top-left y coordinate of the text area.
   * @param data 中文：原始文本字节。
   *             English: Raw text bytes.
   * @param len 中文：文本字节长度。
   *            English: Text byte length.
   * @param color 中文：前景色，RGB565。
   *              English: Foreground color in RGB565.
   * @param bg 中文：背景色，RGB565。
   *           English: Background color in RGB565.
   * @param maxWidth 中文：文本区域最大宽度。
   *                 English: Maximum text area width.
   * @param bottomY 中文：允许绘制的底部边界。
   *                English: Bottom boundary allowed for drawing.
   * @param[out] nextY 中文：输出绘制后的下一行 y 坐标。
   *                   English: Output next y coordinate after drawing.
   */
  void drawBytes(int x, int y, const uint8_t* data, size_t len, uint16_t color,
                 uint16_t bg, int maxWidth, int bottomY, int& nextY);

  /**
   * 中文：估算原始字节文本自动换行后的高度。
   * English: Estimates wrapped height for a raw byte text block.
   *
   * @param data 中文：原始文本字节。
   *             English: Raw text bytes.
   * @param len 中文：文本字节长度。
   *            English: Text byte length.
   * @param maxWidth 中文：文本区域最大宽度。
   *                 English: Maximum text area width.
   * @return 中文：估算高度，单位像素。
   *         English: Estimated height in pixels.
   */
  int measureBytesHeight(const uint8_t* data, size_t len, int maxWidth) const;

 private:
  /**
   * 中文：绘制一个 1-bit 点阵字形。
   * English: Draws one 1-bit bitmap glyph.
   */
  void drawGlyph(int x, int y, int w, int h, const uint8_t* bitmap, uint16_t color,
                 uint16_t bg);

  /**
   * 中文：判断当前位置是否应按 16x16 双字节字模处理。
   * English: Checks whether the current position should use a 16x16 double-byte glyph.
   */
  bool isDoubleByteGlyph(const uint8_t* data, size_t len, size_t index) const;

  /**
   * 中文：在访问屏幕或字库前执行可选的总线准备动作。
   * English: Runs the optional bus preparation hook before touching display or font hardware.
   */
  void prepareBus() const;

  /// 中文：当前文本渲染参数。
  /// English: Current text rendering parameters.
  Config config_;

  /// 中文：可选总线准备回调，不由渲染器释放。
  /// English: Optional bus preparation callback, not owned by the renderer.
  PrepareBusCallback prepareBusCallback_ = nullptr;

  /// 中文：目标屏幕对象，不由渲染器释放。
  /// English: Target display object, not owned by the renderer.
  Arduino_GFX* gfx_ = nullptr;

  /// 中文：可选点阵字模数据源，不由渲染器释放。
  /// English: Optional bitmap font provider, not owned by the renderer.
  IBitmapFontProvider* fontProvider_ = nullptr;
};

}  // namespace WallE
