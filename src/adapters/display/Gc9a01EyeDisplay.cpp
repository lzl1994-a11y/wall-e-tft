#include "adapters/display/Gc9a01EyeDisplay.h"
#include "adapters/display/SharedSpiBus.h"
#include "config/Config.h"
#include "eyeimg.h"

namespace WallE {

namespace {
/// 中文：RGB565 黑色，用于眼睛屏清屏和 GIF 播放前背景。
/// English: RGB565 black, used to clear the eye display and as the pre-GIF background.
constexpr uint16_t kBlack = 0x0000;

Gc9a01Panel::Config makeEyePanelConfig() {
  Gc9a01Panel::Config config;
  config.dcPin = WallEConfig::eysTftDc;
  config.csPin = WallEConfig::eysTftCs;
  config.sckPin = WallEConfig::eysTftSck;
  config.mosiPin = WallEConfig::eysTftMosi;
  config.misoPin = GFX_NOT_DEFINED;
  config.rstPin = WallEConfig::eysTftRst;
  config.spiHost = HSPI;
  config.sharedInterface = false;
  config.width = WallEConfig::kScreenWidth;
  config.height = WallEConfig::kScreenHeight;
  config.rotation = 0;
  config.ips = true;
  config.spiHz = WallEConfig::keyeTftSpiHz;
  return config;
}

GifPlayer::Config makeEyeGifConfig() {
  GifPlayer::Config config;
  config.maxWidth = WallEConfig::kScreenWidth;
  config.clearBeforePlay = true;
  config.clearColor = kBlack;
  return config;
}
}  // namespace

Gc9a01EyeDisplay::Gc9a01EyeDisplay()
    : panel_(makeEyePanelConfig()), gifPlayer_(makeEyeGifConfig()) {}

/**
 * 中文：初始化眼睛屏硬件和 GIF 播放器；此处只清屏，不自动播放动画。
 * English: Initializes eye display hardware and GIF player; it only clears the screen and does not auto-play animation.
 *
 * @param 无 / None.
 * @return 中文：初始化成功返回 true，面板初始化失败返回 false。
 *         English: true on success; false when panel initialization fails.
 */
bool Gc9a01EyeDisplay::begin() {
  ready_ = false;
  beginSharedSpiCsPins();

  deselectSharedSpiDevices();
  if (!panel_.begin()) {
    return false;
  }

  Arduino_GFX* gfx = panel_.gfx();
  if (gfx == nullptr) {
    return false;
  }

  gifPlayer_.setDisplay(gfx);
  gifPlayer_.setPrepareBusCallback(deselectSharedSpiDevices);
  gifPlayer_.begin();

  deselectSharedSpiDevices();
  gfx->fillScreen(kBlack);
  deselectSharedSpiDevices();
  ready_ = true;
  return true;
}

/**
 * 中文：公开的眼睛 zoom 动作入口；当前动作等价于播放一次内置 GIF。
 * English: Public entry for the eye zoom action; currently equivalent to playing the embedded GIF once.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
void Gc9a01EyeDisplay::playZoom() {
  if (!ready_) {
    return;
  }

  gifPlayer_.play(kEyeAnimationGif, kEyeAnimationGifSize);
}

void Gc9a01EyeDisplay::update() {
  if (!ready_) {
    return;
  }

  gifPlayer_.update();
}

}  // namespace WallE
