#include "adapters/display/Gc9a01EyeDisplay.h"
#include "adapters/display/SharedSpiBus.h"
#include "config/Config.h"
#include "eyeimg.h"

namespace WallE {

namespace {
/// 中文：RGB565 黑色，用于清屏。
/// English: RGB565 black, used to clear the display.
constexpr uint16_t kBlack = 0x0000;

/// 中文：GIF 回调是静态函数，使用这个指针访问当前屏幕对象。
/// English: The GIF callback is static, so this pointer gives it access to the current display object.
static Arduino_GC9A01* g_gfx = nullptr;
}  // namespace

/**
 * 中文：初始化眼睛屏硬件；此处只清屏，不自动播放动画。
 * English: Initializes the eye display hardware; it only clears the screen and does not auto-play animation.
 *
 * @param 无 / None.
 * @return 中文：初始化成功返回 true，分配对象或屏幕 begin() 失败返回 false。
 *         English: true on success; false when object allocation or display begin() fails.
 */
bool Gc9a01EyeDisplay::begin() {
  beginSharedSpiCsPins();

  pinMode(WallEConfig::eysTftCs, OUTPUT);
  digitalWrite(WallEConfig::eysTftCs, HIGH);

  bus_ = new Arduino_ESP32SPI(WallEConfig::eysTftDc, WallEConfig::eysTftCs,
                              WallEConfig::eysTftSck, WallEConfig::eysTftMosi,
                              GFX_NOT_DEFINED, HSPI, false);

  gfx_ = new Arduino_GC9A01(bus_, WallEConfig::eysTftRst, 0, true,
                            WallEConfig::kScreenWidth, WallEConfig::kScreenHeight);
  if (bus_ == nullptr || gfx_ == nullptr) {
    return false;
  }
  if (!gfx_->begin(WallEConfig::keyeTftSpiHz)) {
    delete gfx_;
    delete bus_;
    gfx_ = nullptr;
    bus_ = nullptr;
    return false;
  }

  g_gfx = gfx_;
  gif_.begin(LITTLE_ENDIAN_PIXELS);

  deselectSharedSpiDevices();
  gfx_->fillScreen(kBlack);
  deselectSharedSpiDevices();
  return true;
}

/**
 * 中文：AnimatedGIF 解码器的逐行绘制回调，把索引色像素转成 RGB565 后写入屏幕。
 * English: Line-draw callback for AnimatedGIF; converts indexed pixels to RGB565 and writes them to the display.
 *
 * @param pDraw 中文：当前 GIF 行的坐标、宽度、像素索引、调色板和透明色信息。
 *              English: Coordinates, width, pixel indices, palette, and transparency info for the current GIF line.
 * @return 无 / None.
 */
void Gc9a01EyeDisplay::gifDrawCallback(GIFDRAW* pDraw) {
  if (g_gfx == nullptr) {
    return;
  }

  // 中文：s 指向当前 GIF 行的索引像素数据。
  // English: s points to indexed pixel data of the current GIF line.
  uint8_t* s;

  // 中文：d 指向临时 RGB565 行缓存，usPalette 是当前 GIF 调色板。
  // English: d points to the temporary RGB565 line buffer, and usPalette is the current GIF palette.
  uint16_t* d;
  uint16_t* usPalette;

  // 中文：行缓存放在 SRAM 中，便于 Arduino_GFX 直接推送到屏幕。
  // English: Line buffer kept in SRAM so Arduino_GFX can push it directly to the display.
  uint16_t usTemp[320];

  // 中文：x/y/iWidth 分别表示当前行位置和实际绘制宽度。
  // English: x/y/iWidth represent the current line position and actual draw width.
  int x;
  int y;
  int iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth > WallEConfig::kScreenWidth) {
    iWidth = WallEConfig::kScreenWidth;
  }

  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y;
  s = pDraw->pPixels;

  if (pDraw->ucHasTransparency) {
    // 中文：透明行需要跳过透明像素，只把连续的不透明片段推到屏幕。
    // English: Transparent rows skip transparent pixels and push only continuous opaque spans.
    // 中文：pEnd 是当前行结束位置，避免透明扫描越界。
    // English: pEnd marks the end of the current line and prevents overrun while scanning transparency.
    uint8_t* pEnd;

    // 中文：c 保存当前读取到的调色板索引。
    // English: c stores the current palette index read from the GIF line.
    uint8_t c;

    // 中文：ucTransparent 是 GIF 指定的透明色索引。
    // English: ucTransparent is the transparent color index defined by the GIF frame.
    const uint8_t ucTransparent = pDraw->ucTransparent;

    // 中文：x 是当前行内已经处理到的像素偏移。
    // English: x is the processed pixel offset within the current line.
    int x = 0;
    pEnd = s + iWidth;
    while (x < iWidth) {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) {
          s--;
        } else {
          *d++ = usPalette[c];
        }
      }
      if (d > usTemp) {
        g_gfx->draw16bitRGBBitmap(pDraw->iX + x, y, usTemp, d - usTemp, 1);
      }
      x += d - usTemp;
      if (c == ucTransparent) {
        s++;
        x++;
      }
    }
  } else {
    d = usTemp;
    for (x = 0; x < iWidth; x++) {
      d[x] = usPalette[s[x]];
    }
    g_gfx->draw16bitRGBBitmap(pDraw->iX, y, usTemp, iWidth, 1);
  }
}

/**
 * 中文：阻塞播放内置 GIF；收到 eyeaction:zoom 时由 playZoom() 调用。
 * English: Plays the embedded GIF in a blocking way; called by playZoom() on eyeaction:zoom.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
void Gc9a01EyeDisplay::playGif() {
  if (gfx_ == nullptr) {
    return;
  }

  deselectSharedSpiDevices();
  gfx_->fillScreen(kBlack);
  if (gif_.open((uint8_t*)kEyeAnimationGif, kEyeAnimationGifSize, gifDrawCallback)) {
    while (gif_.playFrame(true, nullptr)) {
      // 中文：阻塞播放所有帧；后续如需边收串口边播放，可改成非阻塞状态机。
      // English: Blocks until all frames are played; convert to a non-blocking state machine if Serial input must continue during playback.
    }
    gif_.close();
  }
  deselectSharedSpiDevices();
}

/**
 * 中文：公开的眼睛 zoom 动作入口；当前动作等价于播放一次内置 GIF。
 * English: Public entry for the eye zoom action; currently equivalent to playing the embedded GIF once.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
void Gc9a01EyeDisplay::playZoom() {
  playGif();
}

}  // namespace WallE
