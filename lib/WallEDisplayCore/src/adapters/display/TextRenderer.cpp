#include "adapters/display/TextRenderer.h"
#include <string.h>

namespace WallE {

void TextRenderer::drawBytes(int x, int y, const uint8_t* data, size_t len,
                             uint16_t color, uint16_t bg, int maxWidth, int bottomY,
                             int& nextY) {
  if (gfx_ == nullptr || data == nullptr || len == 0) {
    nextY = y + config_.lineHeight;
    return;
  }

  int cursorX = x;
  int cursorY = y;
  uint8_t bitmap[32] = {0};

  for (size_t i = 0; i < len;) {
    const uint8_t b = data[i];
    if (b == '\r' || b == '\n') {
      cursorX = x;
      cursorY += config_.lineHeight;
      if (cursorY + config_.lineHeight > bottomY) {
        break;
      }
      ++i;
      continue;
    }

    const bool isDoubleByte = isDoubleByteGlyph(data, len, i);
    const int w = isDoubleByte ? config_.doubleByteWidth : config_.asciiWidth;
    if (cursorX != x && cursorX + w > x + maxWidth) {
      cursorX = x;
      cursorY += config_.lineHeight;
      if (cursorY + config_.lineHeight > bottomY) {
        break;
      }
    }

    if (cursorY + config_.lineHeight > bottomY) {
      break;
    }

    if (isDoubleByte) {
      prepareBus();
      if (!fontProvider_->readDoubleByte16x16(b, data[i + 1], bitmap, sizeof(bitmap))) {
        memset(bitmap, 0, sizeof(bitmap));
        const int glyphWidth = min(config_.doubleByteWidth, 16);
        const int glyphHeight = min(config_.lineHeight, 16);
        const int rowBytes = (glyphWidth + 7) / 8;
        for (int row = 0; row < glyphHeight; ++row) {
          for (int col = 0; col < glyphWidth; ++col) {
            if (row == 0 || row == glyphHeight - 1 || col == 0 ||
                col == glyphWidth - 1) {
              bitmap[row * rowBytes + (col >> 3)] |= 0x80 >> (col & 7);
            }
          }
        }
      }
      drawGlyph(cursorX, cursorY, config_.doubleByteWidth, config_.lineHeight, bitmap, color,
                bg);
      i += 2;
    } else {
      const char printable = (b >= 0x20 && b <= 0x7E) ? static_cast<char>(b) : '?';
      prepareBus();
      gfx_->fillRect(cursorX, cursorY, config_.asciiWidth, config_.lineHeight, bg);
      gfx_->setTextColor(color, bg);
      gfx_->setTextSize(1);
      gfx_->setCursor(cursorX, cursorY + config_.asciiYOffset);
      gfx_->print(printable);
      i += 1;
    }
    cursorX += w;
  }
  nextY = min(cursorY + config_.lineHeight, bottomY);
}

int TextRenderer::measureBytesHeight(const uint8_t* data, size_t len, int maxWidth) const {
  if (data == nullptr || len == 0) {
    return config_.lineHeight;
  }

  int cursorX = 0;
  int lines = 1;
  for (size_t i = 0; i < len;) {
    const uint8_t b = data[i];
    if (b == '\r' || b == '\n') {
      cursorX = 0;
      ++lines;
      ++i;
      continue;
    }

    const bool isDoubleByte = isDoubleByteGlyph(data, len, i);
    const int w = isDoubleByte ? config_.doubleByteWidth : config_.asciiWidth;
    if (cursorX != 0 && cursorX + w > maxWidth) {
      cursorX = 0;
      ++lines;
    }

    cursorX += w;
    i += isDoubleByte ? 2 : 1;
  }
  return lines * config_.lineHeight;
}

void TextRenderer::drawGlyph(int x, int y, int w, int h, const uint8_t* bitmap,
                             uint16_t color, uint16_t bg) {
  if (gfx_ == nullptr || bitmap == nullptr) {
    return;
  }
  if (w <= 0 || h <= 0 || w > 16 || h > 16) {
    return;
  }

  const int rowBytes = (w + 7) / 8;
  // The ESP32 SPI-DMA display path is reliable for full-screen frames, but
  // small stack-backed RGB bitmap buffers can be lost between bus operations.
  // Draw each set-bit run directly so font glyphs never depend on that buffer.
  gfx_->fillRect(x, y, w, h, bg);
  for (int row = 0; row < h; ++row) {
    int runStart = -1;
    for (int col = 0; col < w; ++col) {
      const bool dot = bitmap[row * rowBytes + (col >> 3)] & (0x80 >> (col & 7));
      if (dot && runStart < 0) {
        runStart = col;
      }
      if ((!dot || col == w - 1) && runStart >= 0) {
        const int end = dot && col == w - 1 ? col + 1 : col;
        gfx_->drawFastHLine(x + runStart, y + row, end - runStart, color);
        runStart = -1;
      }
    }
  }
}

bool TextRenderer::isDoubleByteGlyph(const uint8_t* data, size_t len, size_t index) const {
  return fontProvider_ != nullptr && data != nullptr && index + 1 < len &&
         data[index] >= config_.doubleByteFirst &&
         data[index + 1] >= config_.doubleByteSecondFirst;
}

void TextRenderer::prepareBus() const {
  if (prepareBusCallback_ != nullptr) {
    prepareBusCallback_();
  }
}

}  // namespace WallE
