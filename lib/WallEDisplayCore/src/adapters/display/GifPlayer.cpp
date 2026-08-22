#include "adapters/display/GifPlayer.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>

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

GifPlayer::~GifPlayer() {
  stop();
  free(frameCanvas_);
  frameCanvas_ = nullptr;
  frameCanvasPixels_ = 0;
}

void GifPlayer::begin() {
  gif_.begin(LITTLE_ENDIAN_PIXELS);
  opened_ = false;
  playing_ = false;
  waitingFinalDelay_ = false;
  nextFrameAtMs_ = 0;

  free(frameCanvas_);
  frameCanvas_ = nullptr;
  frameCanvasPixels_ = 0;
  if (asyncWriter_ != nullptr && config_.maxWidth > 0 &&
      config_.maxHeight > 0) {
    frameCanvasPixels_ =
        static_cast<size_t>(config_.maxWidth) * config_.maxHeight;
    const size_t canvasBytes = frameCanvasPixels_ * sizeof(uint16_t);
    if (psramFound()) {
      frameCanvas_ = static_cast<uint16_t*>(heap_caps_malloc(
          canvasBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    if (frameCanvas_ == nullptr) {
      frameCanvas_ = static_cast<uint16_t*>(malloc(canvasBytes));
    }
    if (frameCanvas_ == nullptr) {
      frameCanvasPixels_ = 0;
    } else {
      for (size_t i = 0; i < frameCanvasPixels_; ++i) {
        frameCanvas_[i] = config_.clearColor;
      }
    }
  }
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
    for (size_t i = 0; i < frameCanvasPixels_; ++i) {
      frameCanvas_[i] = config_.clearColor;
    }
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
    if (config_.loopPlayback && opened_) {
      prepareBus();
      gif_.reset();
      waitingFinalDelay_ = false;
      nextFrameAtMs_ = now;
      return;
    }
    stop();
    return;
  }

  beginAsyncFrame();
  int frameDelayMs = 0;
  const int result = gif_.playFrame(false, &frameDelayMs, nullptr);
  endAsyncFrame();
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
  if (asyncFrameActive_) {
    endAsyncFrame();
  }
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

  if (asyncFrameActive_) {
    drawLineAsync(pDraw);
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

bool GifPlayer::beginAsyncFrame() {
  asyncFrameActive_ = false;
  asyncFrameFailed_ = false;
  chunkBuffer_ = nullptr;
  chunkBufferIndex_ = 0;
  chunkRows_ = 0;

  if (asyncWriter_ == nullptr || frameCanvas_ == nullptr ||
      frameCanvasPixels_ == 0) {
    return false;
  }

  prepareBus();
  asyncFrameActive_ = asyncWriter_->beginFrame();
  return asyncFrameActive_;
}

void GifPlayer::drawLineAsync(GIFDRAW* pDraw) {
  int16_t y = static_cast<int16_t>(pDraw->iY + pDraw->y);
  int16_t x = pDraw->iX;
  int sourceOffset = 0;
  int drawWidth = pDraw->iWidth;

  if (y < 0 || y >= config_.maxHeight || drawWidth <= 0) {
    return;
  }
  if (x < 0) {
    sourceOffset = -x;
    drawWidth -= sourceOffset;
    x = 0;
  }
  if (x >= config_.maxWidth) {
    return;
  }
  if (x + drawWidth > config_.maxWidth) {
    drawWidth = config_.maxWidth - x;
  }
  if (drawWidth <= 0) {
    return;
  }

  const uint8_t* source = pDraw->pPixels + sourceOffset;
  const uint16_t* palette = pDraw->pPalette;
  uint16_t* canvasLine =
      frameCanvas_ + static_cast<size_t>(y) * config_.maxWidth + x;

  if (pDraw->ucHasTransparency) {
    const uint8_t transparent = pDraw->ucTransparent;
    for (int i = 0; i < drawWidth; ++i) {
      const uint8_t colorIndex = source[i];
      if (colorIndex != transparent) {
        canvasLine[i] = palette[colorIndex];
      }
    }
  } else {
    for (int i = 0; i < drawWidth; ++i) {
      canvasLine[i] = palette[source[i]];
    }
  }

  const size_t capacity = asyncWriter_->bufferPixelCapacity();
  const bool contiguous =
      chunkRows_ > 0 && x == chunkX_ && drawWidth == chunkWidth_ &&
      y == chunkY_ + chunkRows_ &&
      static_cast<size_t>(chunkRows_ + 1) * drawWidth <= capacity;
  if (chunkRows_ > 0 && !contiguous && !flushAsyncChunk()) {
    asyncFrameFailed_ = true;
  }
  if (asyncFrameFailed_) {
    return;
  }

  if (chunkRows_ == 0) {
    if (asyncWriter_->bufferCount() == 0) {
      asyncFrameFailed_ = true;
      return;
    }
    chunkBufferIndex_ %= asyncWriter_->bufferCount();
    chunkBuffer_ = asyncWriter_->acquireBuffer(chunkBufferIndex_);
    if (chunkBuffer_ == nullptr ||
        static_cast<size_t>(drawWidth) > capacity) {
      asyncFrameFailed_ = true;
      return;
    }
    chunkX_ = x;
    chunkY_ = y;
    chunkWidth_ = static_cast<int16_t>(drawWidth);
  }

  uint16_t* destination =
      chunkBuffer_ + static_cast<size_t>(chunkRows_) * chunkWidth_;
  for (int i = 0; i < drawWidth; ++i) {
    const uint16_t pixel = canvasLine[i];
    destination[i] = static_cast<uint16_t>((pixel << 8) | (pixel >> 8));
  }
  ++chunkRows_;

  if (static_cast<size_t>(chunkRows_ + 1) * chunkWidth_ > capacity &&
      !flushAsyncChunk()) {
    asyncFrameFailed_ = true;
  }
}

bool GifPlayer::flushAsyncChunk() {
  if (chunkRows_ == 0) {
    return true;
  }
  if (chunkBuffer_ == nullptr ||
      !asyncWriter_->queueRect(
          chunkBufferIndex_, chunkX_, chunkY_, chunkWidth_, chunkRows_,
          static_cast<size_t>(chunkWidth_) * chunkRows_)) {
    chunkRows_ = 0;
    chunkBuffer_ = nullptr;
    return false;
  }

  chunkBufferIndex_ =
      (chunkBufferIndex_ + 1) % asyncWriter_->bufferCount();
  chunkRows_ = 0;
  chunkBuffer_ = nullptr;
  return true;
}

void GifPlayer::endAsyncFrame() {
  if (!asyncFrameActive_) {
    return;
  }
  if (!asyncFrameFailed_ && !flushAsyncChunk()) {
    asyncFrameFailed_ = true;
  }
  asyncWriter_->endFrame();
  asyncFrameActive_ = false;
  chunkRows_ = 0;
  chunkBuffer_ = nullptr;
}

void GifPlayer::prepareBus() const {
  if (prepareBusCallback_ != nullptr) {
    prepareBusCallback_();
  }
}

}  // namespace WallE
