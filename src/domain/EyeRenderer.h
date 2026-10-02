#pragma once

#include <stddef.h>
#include <stdint.h>

namespace WallE {

enum class EyeMood : uint8_t {
  Dot,
  Flame,
  Heart,
};

const char* eyeMoodName(EyeMood mood);

// 中文：仅保存在 RAM，配置与动画均在 UI 循环中更新。
// English: Session-only settings, owned by the UI loop.
struct EyeSettings {
  uint32_t coreColor = 0x00E5FF;
  uint32_t ringColor = 0x00CFE8;
  uint32_t dotColor = 0x00BCD0;
  float brightness = 0.85f;
  float ringBrightness = 0.65f;
  float dotBrightness = 0.45f;
  float minScale = 0.4f;
  float scale = 1.0f;
  float maxScale = 1.5f;
  float glow = 22.0f;
  float range = 0.8f;
  float x = 0.0f;
  float y = 0.0f;
  float breathMs = 2400.0f;
  float blinkMs = 4500.0f;
  int dots = 48;
  bool ring = true;
  EyeMood mood = EyeMood::Dot;
  // Position changes only when the host sends x/y or eyeaction:look.
  bool autoMove = false;
};

// Atomic parsing: invalid input never modifies the current settings.
bool parseEyeSettings(const uint8_t* data, size_t length,
                      const EyeSettings& current, EyeSettings& result);

class EyeRenderer {
 public:
  static constexpr int kWidth = 240;
  static constexpr int kPixels = kWidth * kWidth;
  void makeBase(uint16_t* pixels) const;
  void makeBackdrop(const uint16_t* base, uint16_t* pixels,
                    const EyeSettings& settings) const;
  // Frame composition restores the backdrop first, preventing movement trails.
  void render(const uint16_t* backdrop, uint16_t* frame,
              const EyeSettings& settings, uint32_t now) const;
  void blink(uint32_t now) { blinkAt_ = now; blinking_ = true; }
  void zoom(uint32_t now) { zoomAt_ = now; zooming_ = true; }

 private:
  uint32_t blinkAt_ = 0;
  uint32_t zoomAt_ = 0;
  bool blinking_ = false;
  bool zooming_ = false;
};
}  // namespace WallE
