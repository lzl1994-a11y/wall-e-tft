#include "domain/EyeRenderer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>

using namespace WallE;
bool parse(const char* text, const EyeSettings& in, EyeSettings& out) {
  return parseEyeSettings(reinterpret_cast<const uint8_t*>(text), strlen(text), in, out);
}
void save(const char* path, const std::vector<uint16_t>& pixels) {
  FILE* file = fopen(path, "wb");
  assert(file);
  fprintf(file, "P6\n240 240\n255\n");
  for (uint16_t p : pixels) {
    const unsigned char bytes[] = {
      static_cast<unsigned char>(((p >> 11) & 31) * 255 / 31),
      static_cast<unsigned char>(((p >> 5) & 63) * 255 / 63),
      static_cast<unsigned char>((p & 31) * 255 / 31)};
    assert(fwrite(bytes, 1, 3, file) == 3);
  }
  fclose(file);
}
int main(int argc, char** argv) {
  EyeSettings initial, s;
  assert(parse("color=FF8800,ringColor=00FF00,dotColor=2244FF,scale=1.2,dots=64", initial, s));
  assert(s.coreColor == 0xFF8800 && s.ringColor == 0x00FF00 && s.dotColor == 0x2244FF);
  assert(s.dots == 64 && s.scale == 1.2f);
  EyeSettings moodSettings;
  assert(parse("mood=flame", initial, moodSettings) && moodSettings.mood == EyeMood::Flame);
  assert(parse("mood=LOVE", initial, moodSettings) && moodSettings.mood == EyeMood::Heart);
  assert(parse("mood=normal", initial, moodSettings) && moodSettings.mood == EyeMood::Dot);
  const EyeSettings before = s;
  const char* invalid[] = {"", "scale=0", "dots=65", "dots=1.5", "ring=2",
    "auto=1", "color=GGFFFF", "color=FFFF", "brightness=nan", "range=inf", "x=-27",
    "minScale=1.5,scale=0.4", "unknown=1", "dots=0,", "glow=9junk",
    "brightness=1,ring=2", "breathMs=1", "blinkMs=1", "x= 1", "mood=angry"};
  for (const char* command : invalid) {
    assert(!parse(command, initial, s));
    assert(s.coreColor == before.coreColor && s.dots == before.dots && s.scale == before.scale);
  }
  const uint8_t embeddedNull[] = {'x', '=', '0', 0, ',', 'y', '=', '1'};
  assert(!parseEyeSettings(embeddedNull, sizeof(embeddedNull), initial, s));
  assert(!parseEyeSettings(nullptr, 2, initial, s));
  assert(parse("minScale=0.4,scale=1.5,maxScale=1.5,range=1,x=26,y=-26,glow=30,dots=0,ring=0,breathMs=0,blinkMs=0", initial, s));
  EyeRenderer renderer;
  std::vector<uint16_t> base(EyeRenderer::kPixels), backdrop(base.size());
  std::vector<uint16_t> guarded(base.size() + 2, 0xA55A);
  uint16_t* frame = guarded.data() + 1;
  renderer.makeBase(base.data());
  renderer.makeBackdrop(base.data(), backdrop.data(), s);
  assert(backdrop == base);  // Both outer effects disabled.
  s.brightness = 0;
  renderer.render(backdrop.data(), frame, s, 1000);
  assert(frame[0] == 0 && guarded.front() == 0xA55A && guarded.back() == 0xA55A);
  std::vector<uint16_t> openFrame(frame, frame + base.size());
  assert(memcmp(frame, backdrop.data(), base.size() * sizeof(uint16_t)));
  renderer.blink(1000);
  renderer.render(backdrop.data(), frame, s, 1100);
  assert(memcmp(frame, backdrop.data(), base.size() * sizeof(uint16_t)));
  renderer.render(backdrop.data(), frame, s, 1300);
  assert(memcmp(frame, openFrame.data(), base.size() * sizeof(uint16_t)));
  renderer.render(backdrop.data(), frame, s, 1800);
  assert(!memcmp(frame, openFrame.data(), base.size() * sizeof(uint16_t))); // No trails.
  s = initial; s.autoMove = false; s.breathMs = s.blinkMs = 0;
  renderer.makeBackdrop(base.data(), backdrop.data(), s);
  renderer.render(backdrop.data(), frame, s, 2000);
  assert(frame[120 * 240 + 120] != backdrop[120 * 240 + 120]);
  assert(frame[120 * 240 + 38] != base[120 * 240 + 38]); // Live ring.
  std::vector<uint16_t> fixedFrame(frame, frame + base.size());
  s.autoMove = true;
  renderer.render(backdrop.data(), frame, s, 7000);
  assert(frame[120 * 240 + 120] == fixedFrame[120 * 240 + 120]);
  assert(!memcmp(frame, fixedFrame.data(), base.size() * sizeof(uint16_t)));
  s.ringColor = s.dotColor = s.coreColor = 0xFF8800;
  renderer.makeBackdrop(base.data(), backdrop.data(), s);
  renderer.render(backdrop.data(), frame, s, 2000);
  assert((frame[120 * 240 + 38] >> 11) > (frame[120 * 240 + 38] & 31));
  renderer.zoom(0xFFFFFFF0u);
  renderer.render(backdrop.data(), frame, s, 0x000000A0u);
  assert(guarded.front() == 0xA55A && guarded.back() == 0xA55A);
  s = initial;
  s.breathMs = s.blinkMs = 0;
  renderer.makeBackdrop(base.data(), backdrop.data(), s);
  // At an intermediate blink frame the upper cover starts closer to the
  // blue-dot edge while the lower cover keeps the old travel range.
  renderer.blink(3000);
  renderer.render(backdrop.data(), frame, s, 3090);
  assert(frame[87 * 240 + 80] == frame[80 * 240 + 160]);
  assert(frame[88 * 240 + 80] != frame[87 * 240 + 80]);
  assert(frame[81 * 240 + 160] != frame[80 * 240 + 160]);
  assert(frame[178 * 240 + 80] == frame[171 * 240 + 160]);
  assert(frame[177 * 240 + 80] != frame[178 * 240 + 80]);
  assert(frame[170 * 240 + 160] != frame[171 * 240 + 160]);
  s = initial;
  s.breathMs = s.blinkMs = 0;
  s.mood = EyeMood::Flame;
  renderer.makeBackdrop(base.data(), backdrop.data(), s);
  renderer.render(backdrop.data(), frame, s, 10000);
  std::vector<uint16_t> flameFrame(frame, frame + base.size());
  renderer.render(backdrop.data(), frame, s, 10300);
  assert(memcmp(frame, flameFrame.data(), base.size() * sizeof(uint16_t)));
  s.mood = EyeMood::Heart;
  renderer.makeBackdrop(base.data(), backdrop.data(), s);
  renderer.render(backdrop.data(), frame, s, 11000);
  std::vector<uint16_t> heartFrame(frame, frame + base.size());
  renderer.render(backdrop.data(), frame, s, 11300);
  assert(memcmp(frame, heartFrame.data(), base.size() * sizeof(uint16_t)));
  if (argc == 2) {
    char path[512];
    s = initial; s.autoMove = false; s.breathMs = s.blinkMs = 0;
    renderer.makeBackdrop(base.data(), backdrop.data(), s);
    renderer.render(backdrop.data(), frame, s, 2000);
    snprintf(path, sizeof(path), "%s/eye-live-blue.ppm", argv[1]);
    save(path, std::vector<uint16_t>(frame, frame + base.size()));
    s.coreColor = 0xFF8800; s.ringColor = 0xFF8800; s.dotColor = 0xFF8800;
    s.scale = 1.5f; s.x = 15; s.y = -8;
    renderer.makeBackdrop(base.data(), backdrop.data(), s);
    renderer.render(backdrop.data(), frame, s, 2000);
    snprintf(path, sizeof(path), "%s/eye-live-amber.ppm", argv[1]);
    save(path, std::vector<uint16_t>(frame, frame + base.size()));
    renderer.blink(2000);
    renderer.render(backdrop.data(), frame, s, 2100);
    snprintf(path, sizeof(path), "%s/eye-live-blink.ppm", argv[1]);
    save(path, std::vector<uint16_t>(frame, frame + base.size()));
    s = initial;
    s.autoMove = false;
    s.breathMs = s.blinkMs = 0;
    renderer.makeBackdrop(base.data(), backdrop.data(), s);
    renderer.blink(3000);
    const int blinkTimes[] = {0, 90, 180, 240, 300, 420, 540, 660, 780};
    for (int i = 0; i < static_cast<int>(sizeof(blinkTimes) / sizeof(blinkTimes[0])); ++i) {
      renderer.render(backdrop.data(), frame, s, 3000 + blinkTimes[i]);
      snprintf(path, sizeof(path), "%s/eye-blink-%02d.ppm", argv[1], i);
      save(path, std::vector<uint16_t>(frame, frame + base.size()));
    }
    const int moodTimes[] = {0, 180, 360, 540, 720};
    s.mood = EyeMood::Flame;
    renderer.makeBackdrop(base.data(), backdrop.data(), s);
    for (int i = 0; i < static_cast<int>(sizeof(moodTimes) / sizeof(moodTimes[0])); ++i) {
      renderer.render(backdrop.data(), frame, s, 10000 + moodTimes[i]);
      snprintf(path, sizeof(path), "%s/eye-flame-%02d.ppm", argv[1], i);
      save(path, std::vector<uint16_t>(frame, frame + base.size()));
    }
    s.mood = EyeMood::Heart;
    renderer.makeBackdrop(base.data(), backdrop.data(), s);
    for (int i = 0; i < static_cast<int>(sizeof(moodTimes) / sizeof(moodTimes[0])); ++i) {
      renderer.render(backdrop.data(), frame, s, 11000 + moodTimes[i]);
      snprintf(path, sizeof(path), "%s/eye-heart-%02d.ppm", argv[1], i);
      save(path, std::vector<uint16_t>(frame, frame + base.size()));
    }
  }
  puts("PASS: atomic settings, malformed/boundary inputs, frame guards, blink, no trails, color, timer wrap");
}
