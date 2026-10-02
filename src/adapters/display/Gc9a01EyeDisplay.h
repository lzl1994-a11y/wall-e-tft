#pragma once

#include "adapters/display/Gc9a01Panel.h"
#include "ports/IEyeDisplayPort.h"

namespace WallE {
// State and buffers are owned exclusively by the UI loop.
class Gc9a01EyeDisplay : public IEyeDisplayPort {
 public:
  Gc9a01EyeDisplay();
  ~Gc9a01EyeDisplay() override;
  bool begin();
  void playZoom() override;
  void blink() override;
  bool configure(const uint8_t* data, size_t length) override;
  const EyeSettings& settings() const override { return settings_; }
  bool renderOk() const override { return ready_ && renderOk_; }
  void update() override;
  bool asyncDmaReady() const { return ready_ && writer_ != nullptr; }

 private:
  void rebuildBackdrop();
  bool pushFrame();
  Gc9a01Panel panel_;
  IAsyncBitmapWriter* writer_ = nullptr;
  EyeRenderer renderer_;
  EyeSettings settings_;
  uint16_t* backdrop_ = nullptr;
  uint16_t* frame_ = nullptr;
  uint32_t lastFrameAt_ = 0;
  bool ready_ = false;
  bool dirty_ = true;
  bool renderOk_ = false;
};
}  // namespace WallE
