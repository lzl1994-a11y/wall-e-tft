#include "adapters/display/GifPlayer.h"
#include <Arduino.h>

namespace WallE {

namespace {
/// 中文：GIF 单行临时缓存最大像素数；当前 240x240 眼睛动画足够使用。
/// English: Maximum pixels in the temporary GIF line buffer; enough for the current 240x240 eye animation.
constexpr int kMaxGifLinePixels = 320;

/// 中文：AnimatedGIF 只支持静态 C 回调，因此播放期间用该指针转发到当前实例。
/// English: AnimatedGIF only supports a static C callback, so this pointer forwards to the active instance during playback.
GifPlayer* g_activePlayer = nullptr;
}  // namespace

GifPlayer::GifPlayer(const Config& config) : config_(config) {}

void GifPlayer::begin() {
  gif_.begin(LITTLE_ENDIAN_PIXELS);
  opened_ = false;
  playing_ = false;
  waitingFinalDelay_ = false;
  nextFrameAtMs_ = 0;
}

bool GifPlayer::play(const uint8_t* data, size_t size) {
  if (gfx_ == nullptr || data == nullptr || size == 0) {
    return false;
  }

  stop();
  prepareBus();
  g_activePlayer = this;
  gif_.begin(LITTLE_ENDIAN_PIXELS);
  if (config_.clearBeforePlay) {
    gfx_->fillScreen(config_.clearColor);
  }

  opened_ = gif_.open((uint8_t*)data, size, gifDrawCallback);
  if (!opened_) {
    g_activePlayer = nullptr;
    prepareBus();
    return false;
  }
  playing_ = true;
  waitingFinalDelay_ = false;
  nextFrameAtMs_ = millis();
  return true;
}

void GifPlayer::update() {
  if (!playing_) {
    return;
  }

  const uint32_t now = millis();
  if (static_cast<int32_t>(now - nextFrameAtMs_) < 0) {
    return;
  }

  if (waitingFinalDelay_) {
    stop();
    return;
  }

  int frameDelayMs = 0;
  const int result = gif_.playFrame(false, &frameDelayMs, nullptr);
  if (result < 0) {
    stop();
    return;
  }

  if (frameDelayMs < 0) {
    frameDelayMs = 0;
  }
  nextFrameAtMs_ = now + static_cast<uint32_t>(frameDelayMs);

  if (result == 0) {
    waitingFinalDelay_ = true;
  }
}

void GifPlayer::stop() {
  if (opened_) {
    gif_.close();
  }
  opened_ = false;
  playing_ = false;
  waitingFinalDelay_ = false;
  nextFrameAtMs_ = 0;
  if (g_activePlayer == this) {
    g_activePlayer = nullptr;
  }
  prepareBus();
}

void GifPlayer::gifDrawCallback(GIFDRAW* pDraw) {
  if (g_activePlayer == nullptr) {
    return;
  }
  g_activePlayer->drawLine(pDraw);
}

void GifPlayer::drawLine(GIFDRAW* pDraw) {
  if (gfx_ == nullptr || pDraw == nullptr) {
    return;
  }

  int drawWidth = pDraw->iWidth;
  if (drawWidth > config_.maxWidth) {
    drawWidth = config_.maxWidth;
  }
  if (drawWidth > kMaxGifLinePixels) {
    drawWidth = kMaxGifLinePixels;
  }
  if (drawWidth <= 0) {
    return;
  }

  uint8_t* source = pDraw->pPixels;
  uint16_t* palette = pDraw->pPalette;
  uint16_t lineBuffer[kMaxGifLinePixels];
  const int y = pDraw->iY + pDraw->y;

  if (pDraw->ucHasTransparency) {
    // 中文：透明行只推送连续的不透明片段，避免把透明色写成背景色。
    // English: Transparent rows only push continuous opaque spans to avoid drawing transparent pixels as background.
    const uint8_t transparent = pDraw->ucTransparent;
    uint8_t* rowEnd = source + drawWidth;
    int x = 0;
    while (x < drawWidth) {
      uint8_t colorIndex = transparent - 1;
      uint16_t* dest = lineBuffer;
      while (colorIndex != transparent && source < rowEnd) {
        colorIndex = *source++;
        if (colorIndex == transparent) {
          --source;
        } else {
          *dest++ = palette[colorIndex];
        }
      }

      const int spanWidth = dest - lineBuffer;
      if (spanWidth > 0) {
        prepareBus();
        gfx_->draw16bitRGBBitmap(pDraw->iX + x, y, lineBuffer, spanWidth, 1);
      }
      x += spanWidth;

      if (colorIndex == transparent) {
        ++source;
        ++x;
      }
    }
    return;
  }

  for (int x = 0; x < drawWidth; ++x) {
    lineBuffer[x] = palette[source[x]];
  }
  prepareBus();
  gfx_->draw16bitRGBBitmap(pDraw->iX, y, lineBuffer, drawWidth, 1);
}

void GifPlayer::prepareBus() const {
  if (prepareBusCallback_ != nullptr) {
    prepareBusCallback_();
  }
}

}  // namespace WallE
