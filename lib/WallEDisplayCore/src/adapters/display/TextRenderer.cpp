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
  uint16_t pixels[16 * 16] = {0};
  for (int row = 0; row < h; ++row) {
    for (int col = 0; col < w; ++col) {
      const bool dot = bitmap[row * rowBytes + (col >> 3)] & (0x80 >> (col & 7));
      pixels[row * w + col] = dot ? color : bg;
    }
  }
  prepareBus();
  gfx_->draw16bitRGBBitmap(x, y, pixels, w, h);
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
