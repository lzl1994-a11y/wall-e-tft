#include "adapters/display/Gc9a01EyeDisplay.h"
#include "adapters/display/SharedSpiBus.h"
#include "adapters/log/SerialOutput.h"
#include "config/Config.h"
#include <esp_heap_caps.h>
#include <stdlib.h>

namespace WallE {
namespace {
Gc9a01Panel::Config makeEyePanelConfig() {
  Gc9a01Panel::Config config;
  config.dcPin = WallEConfig::eysTftDc;
  config.csPin = WallEConfig::eysTftCs;
  config.sckPin = WallEConfig::eysTftSck;
  config.mosiPin = WallEConfig::eysTftMosi;
  config.misoPin = GFX_NOT_DEFINED;
  config.rstPin = WallEConfig::eysTftRst;
  config.spiHost = SPI3_HOST;
  config.sharedInterface = true;
  config.width = WallEConfig::kScreenWidth;
  config.height = WallEConfig::kScreenHeight;
  config.rotation = 0;
  config.ips = true;
  config.spiHz = WallEConfig::keyeTftSpiHz;
  return config;
}
uint16_t* allocateCanvas() {
  const size_t bytes = EyeRenderer::kPixels * sizeof(uint16_t);
  auto* pixels = static_cast<uint16_t*>(heap_caps_malloc(
      bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!pixels) pixels = static_cast<uint16_t*>(malloc(bytes));
  return pixels;
}
}  // namespace

Gc9a01EyeDisplay::Gc9a01EyeDisplay() : panel_(makeEyePanelConfig()) {}
Gc9a01EyeDisplay::~Gc9a01EyeDisplay() {
  free(backdrop_);
  free(frame_);
}

bool Gc9a01EyeDisplay::begin() {
  if (ready_) return true;
  beginSharedSpiCsPins();
  deselectSharedSpiDevices();
  if (!panel_.begin() || !panel_.gfx()) return false;
  free(backdrop_);
  free(frame_);
  backdrop_ = allocateCanvas();
  frame_ = allocateCanvas();
  if (!backdrop_ || !frame_) {
    free(backdrop_);
    free(frame_);
    backdrop_ = frame_ = nullptr;
    return false;
  }
  writer_ = panel_.asyncWriter();
  rebuildBackdrop();
  ready_ = true;
  dirty_ = true;
  update();
  return renderOk_;
}

void Gc9a01EyeDisplay::playZoom() {
  if (!ready_) return;
  renderer_.zoom(millis());
  dirty_ = true;
}
void Gc9a01EyeDisplay::blink() {
  if (!ready_) return;
  renderer_.blink(millis());
  dirty_ = true;
}
bool Gc9a01EyeDisplay::configure(const uint8_t* data, size_t length) {
  EyeSettings candidate;
  if (!ready_ || !parseEyeSettings(data, length, settings_, candidate)) return false;
  const bool backdropChanged = candidate.ringColor != settings_.ringColor ||
      candidate.dotColor != settings_.dotColor || candidate.ring != settings_.ring ||
      candidate.dots != settings_.dots ||
      candidate.ringBrightness != settings_.ringBrightness ||
      candidate.dotBrightness != settings_.dotBrightness;
  settings_ = candidate;
  if (backdropChanged) rebuildBackdrop();
  dirty_ = true;
  return true;
}
void Gc9a01EyeDisplay::rebuildBackdrop() {
  renderer_.makeBase(backdrop_);
  renderer_.makeBackdrop(backdrop_, backdrop_, settings_);
}
bool Gc9a01EyeDisplay::pushFrame() {
  deselectSharedSpiDevices();
  if (!writer_) {
    // Existing synchronous panel path, explicitly reported at boot.
    panel_.gfx()->draw16bitRGBBitmap(0, 0, frame_, 240, 240);
    return true;
  }
  if (!writer_->beginFrame()) return false;
  bool ok = writer_->bufferCount() > 0 && writer_->bufferPixelCapacity() >= 240;
  size_t slot = 0;
  for (int y = 0; y < 240 && ok;) {
    const int rows = min(240 - y, static_cast<int>(writer_->bufferPixelCapacity() / 240));
    uint16_t* destination = writer_->acquireBuffer(slot);
    if (!destination) { ok = false; break; }
    const size_t count = static_cast<size_t>(rows) * 240;
    for (size_t i = 0; i < count; ++i) {
      const uint16_t pixel = frame_[static_cast<size_t>(y) * 240 + i];
      destination[i] = static_cast<uint16_t>((pixel << 8) | (pixel >> 8));
    }
    ok = writer_->queueRect(slot, 0, y, 240, rows, count);
    slot = (slot + 1) % writer_->bufferCount();
    y += rows;
  }
  writer_->endFrame();
  return ok;
}
void Gc9a01EyeDisplay::update() {
  if (!ready_) return;
  const uint32_t now = millis();
  if (!dirty_ && now - lastFrameAt_ < 33) return;
  lastFrameAt_ = now;
  renderer_.render(backdrop_, frame_, settings_, now);
  renderOk_ = pushFrame();
  dirty_ = false;
  if (!renderOk_) {
    ready_ = false;
    serialLogLine("ERROR", "eye render transfer failed; restart required");
  }
}
}  // namespace WallE
