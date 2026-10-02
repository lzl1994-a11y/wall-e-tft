#include "domain/EyeRenderer.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace WallE {
namespace {
float clamp(float v, float lo, float hi) { return fmaxf(lo, fminf(hi, v)); }
uint16_t rgb(float r, float g, float b) {
  return (static_cast<uint16_t>(clamp(r, 0, 255)) >> 3) << 11 |
         (static_cast<uint16_t>(clamp(g, 0, 255)) >> 2) << 5 |
         (static_cast<uint16_t>(clamp(b, 0, 255)) >> 3);
}
uint16_t light(uint32_t color, float strength) {
  return rgb(((color >> 16) & 255) * strength,
             ((color >> 8) & 255) * strength, (color & 255) * strength);
}
uint16_t addLight(uint16_t base, uint32_t color, float strength, float white) {
  return rgb(((base >> 11) & 31) * 255.0f / 31 +
                 ((color >> 16) & 255) * strength + white,
             ((base >> 5) & 63) * 255.0f / 63 +
                 ((color >> 8) & 255) * strength + white,
             (base & 31) * 255.0f / 31 + (color & 255) * strength + white);
}
uint16_t eyelidColor() { return rgb(16, 20, 22); }
// The two card-like covers are tilted 5 degrees counter-clockwise.
constexpr float kBlinkCoverSin = 0.087155744f;
constexpr float kBlinkCoverCos = 0.996194720f;
// A small resting overlap hides only the outer edge of the upper blue dots.
constexpr float kUpperOpenGap = 70.0f;
constexpr uint32_t kBlinkCloseMs = 180;
constexpr uint32_t kBlinkHoldMs = 120;
constexpr uint32_t kBlinkOpenMs = 300;
constexpr uint32_t kBlinkDurationMs =
    kBlinkCloseMs + kBlinkHoldMs + kBlinkOpenMs;
bool equalsMoodText(const char* left, const char* right) {
  while (*left && *right) {
    char a = *left++;
    char b = *right++;
    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
    if (a != b) return false;
  }
  return *left == 0 && *right == 0;
}
bool parseMoodText(const char* text, EyeMood& out) {
  if (equalsMoodText(text, "dot") || equalsMoodText(text, "normal")) {
    out = EyeMood::Dot;
    return true;
  }
  if (equalsMoodText(text, "flame") || equalsMoodText(text, "fire")) {
    out = EyeMood::Flame;
    return true;
  }
  if (equalsMoodText(text, "heart") || equalsMoodText(text, "love")) {
    out = EyeMood::Heart;
    return true;
  }
  return false;
}
bool hexColor(const char* text, uint32_t& out) {
  if (strlen(text) != 6) return false;
  uint32_t value = 0;
  for (int i = 0; i < 6; ++i) {
    const char c = text[i];
    const int n = c >= '0' && c <= '9' ? c - '0' :
                  c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                  c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
    if (n < 0) return false;
    value = value * 16 + n;
  }
  out = value;
  return true;
}
}  // namespace

const char* eyeMoodName(EyeMood mood) {
  switch (mood) {
    case EyeMood::Flame: return "flame";
    case EyeMood::Heart: return "heart";
    case EyeMood::Dot:
    default: return "dot";
  }
}

bool parseEyeSettings(const uint8_t* data, size_t length,
                      const EyeSettings& current, EyeSettings& result) {
  if (!data || length == 0 || length > 240) return false;
  char text[241];
  for (size_t i = 0; i < length; ++i) {
    if (data[i] < 33 || data[i] > 126) return false;
    text[i] = static_cast<char>(data[i]);
  }
  text[length] = 0;
  EyeSettings candidate = current;
  char* field = text;
  while (*field) {
    char* comma = strchr(field, ',');
    if (comma) *comma = 0;
    char* equals = strchr(field, '=');
    if (!equals || equals == field || !equals[1]) return false;
    *equals = 0;
    const char* value = equals + 1;
    uint32_t* color = !strcmp(field, "color") ? &candidate.coreColor :
                      !strcmp(field, "ringColor") ? &candidate.ringColor :
                      !strcmp(field, "dotColor") ? &candidate.dotColor : nullptr;
    if (color) {
      if (!hexColor(value, *color)) return false;
      if (!strcmp(field, "color")) {
        candidate.ringColor = candidate.dotColor = candidate.coreColor;
      }
    } else if (!strcmp(field, "mood")) {
      if (!parseMoodText(value, candidate.mood)) return false;
    } else {
      char* end = nullptr;
      const float n = strtof(value, &end);
      if (end == value || *end || !isfinite(n)) return false;
      float* target = nullptr;
      float lo = 0, hi = 1;
      if (!strcmp(field, "brightness")) target = &candidate.brightness;
      else if (!strcmp(field, "ringBrightness")) target = &candidate.ringBrightness;
      else if (!strcmp(field, "dotBrightness")) target = &candidate.dotBrightness;
      else if (!strcmp(field, "range")) target = &candidate.range;
      else if (!strcmp(field, "scale")) { target = &candidate.scale; lo = 0.4f; hi = 1.5f; }
      else if (!strcmp(field, "minScale")) { target = &candidate.minScale; lo = 0.4f; hi = 1.5f; }
      else if (!strcmp(field, "maxScale")) { target = &candidate.maxScale; lo = 0.4f; hi = 1.5f; }
      else if (!strcmp(field, "glow")) { target = &candidate.glow; lo = 8; hi = 30; }
      else if (!strcmp(field, "x")) { target = &candidate.x; lo = -26; hi = 26; }
      else if (!strcmp(field, "y")) { target = &candidate.y; lo = -26; hi = 26; }
      else if (!strcmp(field, "breathMs")) { target = &candidate.breathMs; hi = 10000; if (n != 0) lo = 500; }
      else if (!strcmp(field, "blinkMs")) { target = &candidate.blinkMs; hi = 15000; if (n != 0) lo = 1000; }
      else if (!strcmp(field, "dots")) {
        if (n < 0 || n > 64 || floorf(n) != n) return false;
        candidate.dots = static_cast<int>(n);
      } else if (!strcmp(field, "ring") || !strcmp(field, "auto")) {
        if (n != 0 && n != 1) return false;
        if (!strcmp(field, "ring")) candidate.ring = n == 1;
        else if (n != 0) return false;
        else candidate.autoMove = false;
      } else return false;
      if (target) {
        if (n < lo || n > hi) return false;
        *target = n;
      }
    }
    if (!comma) break;
    field = comma + 1;
    if (!*field) return false;
  }
  if (candidate.minScale > candidate.scale || candidate.scale > candidate.maxScale)
    return false;
  result = candidate;
  return true;
}

void EyeRenderer::makeBase(uint16_t* pixels) const {
  for (int y = 0; y < kWidth; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      const float dx = x - 119.5f, dy = y - 119.5f;
      const float r = sqrtf(dx * dx + dy * dy);
      float v = 0, b = 0;
      if (r < 108 && r >= 88) {
        const float bevel = sinf((r - 88) / 20 * 3.14159265f);
        const float top = clamp(0.55f - dy / 220, 0, 1);
        v = 13 + 37 * bevel * top;
        if (r > 105.5f) v += 28 * top * (108 - r) / 2.5f;
        b = 2;
      } else if (r < 86) {
        v = 2 + 5 * r / 86;
        b = 4 + 4 * r / 86;
        // A single subdued arc reflection gives the glass its lens identity.
        const float arc = clamp(1 - fabsf(r - 65) / 13, 0, 1);
        const float upperLeft = clamp((-dx - dy - 46) / 65, 0, 1);
        v += 27 * arc * upperLeft;
        b += 9 * arc * upperLeft;
      }
      pixels[y * kWidth + x] = rgb(v, v + b * 0.5f, v + b);
    }
  }
}

void EyeRenderer::makeBackdrop(const uint16_t* base, uint16_t* pixels,
                              const EyeSettings& s) const {
  if (pixels != base) memcpy(pixels, base, kPixels * sizeof(uint16_t));
  if (s.ring) {
    for (int y = 30; y < 210; ++y) for (int x = 30; x < 210; ++x) {
      const float dx = x - 119.5f, dy = y - 119.5f;
      const float distance = fabsf(sqrtf(dx * dx + dy * dy) - 82);
      if (distance < 3) pixels[y * kWidth + x] = addLight(
          pixels[y * kWidth + x], s.ringColor,
          s.ringBrightness * clamp(1 - distance / 3, 0, 1), 0);
    }
  }
  for (int i = 0; i < s.dots; ++i) {
    const float a = i * 6.2831853f / s.dots;
    const int cx = static_cast<int>(119.5f + 71 * cosf(a));
    const int cy = static_cast<int>(119.5f + 71 * sinf(a));
    for (int y = cy - 1; y <= cy + 1; ++y)
      for (int x = cx - 1; x <= cx + 1; ++x)
        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= 1)
          pixels[y * kWidth + x] = light(s.dotColor, s.dotBrightness);
  }
}

struct MoodPoint {
  float x;
  float y;
};

bool insidePolygon(float x, float y, const MoodPoint* points, int count) {
  bool inside = false;
  for (int i = 0, j = count - 1; i < count; j = i++) {
    const bool crosses = (points[i].y > y) != (points[j].y > y);
    if (crosses && x < (points[j].x - points[i].x) *
        (y - points[i].y) / (points[j].y - points[i].y) + points[i].x) {
      inside = !inside;
    }
  }
  return inside;
}

void drawMoodGlow(uint16_t* frame, float cx, float cy, float scale,
                  float brightness, float glow, uint32_t color,
                  uint32_t now) {
  const float phase = (now % 900) * 6.2831853f / 900.0f;
  const float halo = glow * scale * (0.92f + 0.08f * sinf(phase));
  if (halo <= 0) return;
  const int extent = static_cast<int>(halo + 1);
  for (int y = static_cast<int>(cy) - extent; y <= cy + extent; ++y) {
    for (int x = static_cast<int>(cx) - extent; x <= cx + extent; ++x) {
      if (x < 0 || y < 0 || x >= EyeRenderer::kWidth || y >= EyeRenderer::kWidth)
        continue;
      const float dx = x - cx, dy = y - cy;
      const float rr = dx * dx + dy * dy;
      if (rr >= halo * halo) continue;
      const float fade = 1 - rr / (halo * halo);
      frame[y * EyeRenderer::kWidth + x] = addLight(
          frame[y * EyeRenderer::kWidth + x], color,
          brightness * 0.42f * fade * fade * fade, 0);
    }
  }
}

void drawFlame(uint16_t* frame, float cx, float cy, float scale,
               float brightness, float glow, uint32_t now) {
  const float phase = (now % 900) * 6.2831853f / 900.0f;
  const float wobble = 1.2f * scale * sinf(phase);
  const float size = 13.0f * scale * (0.96f + 0.06f * sinf(phase * 2));
  const float flameX = cx + wobble;
  const float flameY = cy + 0.5f * scale * sinf(phase * 1.3f);
  const MoodPoint outer[] = {
      {-0.08f, -1.15f}, {-0.28f, -0.66f}, {-0.60f, -0.34f},
      {-0.76f, 0.18f}, {-0.55f, 0.76f}, {-0.15f, 1.10f},
      {0.28f, 0.98f}, {0.62f, 0.57f}, {0.73f, 0.10f},
      {0.52f, -0.28f}, {0.25f, -0.58f}, {0.18f, -0.05f},
      {0.03f, 0.32f}, {-0.18f, 0.02f}, {-0.10f, -0.38f}};
  const MoodPoint inner[] = {
      {0.02f, -0.62f}, {-0.22f, -0.08f}, {-0.34f, 0.38f},
      {-0.05f, 0.78f}, {0.30f, 0.45f}, {0.34f, 0.05f},
      {0.15f, -0.26f}};
  drawMoodGlow(frame, flameX, flameY, scale, brightness, glow, 0xFF4A16, now);
  const int extent = static_cast<int>(size * 1.35f + 2);
  for (int y = static_cast<int>(flameY) - extent; y <= flameY + extent; ++y) {
    for (int x = static_cast<int>(flameX) - extent; x <= flameX + extent; ++x) {
      if (x < 0 || y < 0 || x >= EyeRenderer::kWidth || y >= EyeRenderer::kWidth)
        continue;
      const float localX = (x - flameX) / size;
      const float localY = (y - flameY) / size;
      if (!insidePolygon(localX, localY, outer,
                         static_cast<int>(sizeof(outer) / sizeof(outer[0]))))
        continue;
      const bool isInner = insidePolygon(
          localX, localY, inner,
          static_cast<int>(sizeof(inner) / sizeof(inner[0])));
      const uint32_t color = isInner ? 0xFFFFA8 : 0xFF5A16;
      const float strength = brightness * (isInner ? 1.0f : 0.82f);
      frame[y * EyeRenderer::kWidth + x] = addLight(
          frame[y * EyeRenderer::kWidth + x], color, strength,
          isInner ? 18.0f : 0);
    }
  }
}

void drawHeart(uint16_t* frame, float cx, float cy, float scale,
               float brightness, float glow, uint32_t now) {
  const float phase = (now % 1000) * 6.2831853f / 1000.0f;
  const float pulse = 1.0f + 0.07f * sinf(phase);
  const float size = 12.0f * scale * pulse;
  drawMoodGlow(frame, cx, cy, scale * pulse, brightness, glow, 0xFF174F, now);
  const int extent = static_cast<int>(size * 1.35f + 2);
  for (int y = static_cast<int>(cy) - extent; y <= cy + extent; ++y) {
    for (int x = static_cast<int>(cx) - extent; x <= cx + extent; ++x) {
      if (x < 0 || y < 0 || x >= EyeRenderer::kWidth || y >= EyeRenderer::kWidth)
        continue;
      const float hx = (x - cx) / size;
      const float hy = -(y - cy) / size;
      const float a = hx * hx + hy * hy - 1.0f;
      if (a * a * a - hx * hx * hy * hy * hy > 0) continue;
      const bool highlight = hx < -0.20f && hy > 0.10f;
      frame[y * EyeRenderer::kWidth + x] = addLight(
          frame[y * EyeRenderer::kWidth + x],
          highlight ? 0xFFFFD8 : 0xFF2D6B,
          brightness * (highlight ? 1.0f : 0.9f),
          highlight ? 26.0f : 0);
    }
  }
}

void EyeRenderer::render(const uint16_t* backdrop, uint16_t* frame,
                         const EyeSettings& s, uint32_t now) const {
  memcpy(frame, backdrop, kPixels * sizeof(uint16_t));
  // Position is intentionally static. The host owns gaze updates via x/y.
  float px = s.x, py = s.y;
  const float length = sqrtf(px * px + py * py);
  if (length > 26) { px *= 26 / length; py *= 26 / length; }
  float blinkProgress = 0;
  uint32_t blinkElapsed = 1000;
  if (s.blinkMs > 0) blinkElapsed = now % static_cast<uint32_t>(s.blinkMs);
  if (blinking_ && now - blinkAt_ < kBlinkDurationMs)
    blinkElapsed = now - blinkAt_;
  if (blinkElapsed < kBlinkCloseMs)
    blinkProgress = blinkElapsed / static_cast<float>(kBlinkCloseMs);
  else if (blinkElapsed < kBlinkCloseMs + kBlinkHoldMs)
    blinkProgress = 1;
  else if (blinkElapsed < kBlinkDurationMs)
    blinkProgress = 1 - (blinkElapsed - kBlinkCloseMs - kBlinkHoldMs) /
        static_cast<float>(kBlinkOpenMs);
  float scale = s.scale;
  if (zooming_ && now - zoomAt_ < 900) {
    const float elapsed = static_cast<float>(now - zoomAt_);
    if (elapsed < 350) scale += (s.maxScale - s.scale) * elapsed / 350;
    else if (elapsed < 650) scale = s.maxScale +
        (s.minScale - s.maxScale) * (elapsed - 350) / 300;
    else scale = s.minScale + (s.scale - s.minScale) * (elapsed - 650) / 250;
  }
  float brightness = s.brightness;
  if (s.breathMs > 0) brightness *= 0.82f + 0.18f * sinf(
      (now % static_cast<uint32_t>(s.breathMs)) * 6.2831853f / s.breathMs);
  const float cx = 119.5f + px, cy = 119.5f + py;
  if (s.mood == EyeMood::Flame) {
    drawFlame(frame, cx, cy, scale, brightness, s.glow, now);
  } else if (s.mood == EyeMood::Heart) {
    drawHeart(frame, cx, cy, scale, brightness, s.glow, now);
  } else {
    const float radius = 6 * scale;
    const float halo = s.glow * scale;
    const int extent = static_cast<int>(halo + 1);
    for (int y = static_cast<int>(cy) - extent; y <= cy + extent; ++y) {
      for (int x = static_cast<int>(cx) - extent; x <= cx + extent; ++x) {
        if (x < 0 || y < 0 || x >= kWidth || y >= kWidth) continue;
        const float dx = x - cx, dy = y - cy;
        const float rr = dx * dx + dy * dy;
        if (rr >= halo * halo) continue;
        const float fade = 1 - rr / (halo * halo);
        const float core = clamp(1 - rr / (radius * radius), 0, 1);
        const float strength = brightness * (0.55f * fade * fade * fade + core);
        const int index = y * kWidth + x;
        frame[index] = addLight(frame[index], s.coreColor, strength,
                                core * core * brightness * 150);
      }
    }
  }
  // Two straight-edged cover plates slide in from the physical screen edge,
  // like two cards held above and below the lens. They are tilted 5 degrees
  // counter-clockwise. Clip them to the circular display so their outer edge
  // follows the panel while each inner edge stays a straight line. The upper
  // plate has a small resting overlap; the lower plate keeps its old start.
  const float eyeRadius = 108.0f;
  const float upperGap = kUpperOpenGap * (1 - blinkProgress);
  const float lowerGap = eyeRadius * (1 - blinkProgress);
  for (int y = 0; y < kWidth; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      const float dx = x - 119.5f, dy = y - 119.5f;
      if (dx * dx + dy * dy >= eyeRadius * eyeRadius) continue;
      // This is the y-coordinate in the cover's rotated frame. A positive
      // visual angle is counter-clockwise (screen y grows downward).
      const float rotatedY = kBlinkCoverSin * dx + kBlinkCoverCos * dy;
      if (rotatedY < -upperGap || rotatedY > lowerGap)
        frame[y * kWidth + x] = eyelidColor();
    }
  }
}
}  // namespace WallE
