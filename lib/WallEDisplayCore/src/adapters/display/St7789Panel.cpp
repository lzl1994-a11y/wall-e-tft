#include "adapters/display/St7789Panel.h"

namespace WallE {

namespace {

int16_t dmaXOffset(const St7789Panel::Config& config) {
  switch (config.rotation) {
    case 1:
    case 7:
      return config.rowOffset1;
    case 2:
    case 6:
      return config.colOffset2;
    case 3:
    case 5:
      return config.rowOffset2;
    default:
      return config.colOffset1;
  }
}

int16_t dmaYOffset(const St7789Panel::Config& config) {
  switch (config.rotation) {
    case 1:
    case 7:
      return config.colOffset2;
    case 2:
    case 6:
      return config.rowOffset2;
    case 3:
    case 5:
      return config.colOffset1;
    default:
      return config.rowOffset1;
  }
}

}  // namespace

St7789Panel::St7789Panel(const Config& config)
    : config_(config),
      asyncWriter_(config.dcPin, config.csPin, config.spiHost,
                   config.spiHz, dmaXOffset(config), dmaYOffset(config)) {}

St7789Panel::~St7789Panel() {
  asyncWriter_.end();
  delete gfx_;
  delete bus_;
}

bool St7789Panel::begin() {
  ready_ = false;
  if (gfx_ != nullptr) {
    ready_ = gfx_->begin(config_.spiHz);
    return ready_;
  }

  bus_ = new Arduino_ESP32SPIDMA(
      config_.dcPin, config_.csPin, config_.sckPin, config_.mosiPin,
      config_.misoPin, config_.spiHost, config_.sharedInterface);
  if (bus_ == nullptr) {
    return false;
  }

  gfx_ = new Arduino_ST7789(bus_, config_.rstPin, config_.rotation, config_.ips,
                            config_.width, config_.height, config_.colOffset1,
                            config_.rowOffset1, config_.colOffset2, config_.rowOffset2);
  if (gfx_ == nullptr) {
    delete bus_;
    bus_ = nullptr;
    return false;
  }

  ready_ = gfx_->begin(config_.spiHz);
  if (!ready_) {
    delete gfx_;
    delete bus_;
    gfx_ = nullptr;
    bus_ = nullptr;
    return false;
  }
  asyncWriter_.begin();
  return ready_;
}

}  // namespace WallE
