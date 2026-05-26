#include "adapters/display/St7789Panel.h"

namespace WallE {

St7789Panel::St7789Panel(const Config& config) : config_(config) {}

St7789Panel::~St7789Panel() {
  delete gfx_;
  delete bus_;
}

bool St7789Panel::begin() {
  ready_ = false;
  if (gfx_ != nullptr) {
    ready_ = gfx_->begin(config_.spiHz);
    return ready_;
  }

  bus_ = new Arduino_HWSPI(config_.dcPin, config_.csPin, config_.sckPin, config_.mosiPin,
                           config_.misoPin);
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
  }
  return ready_;
}

}  // namespace WallE
