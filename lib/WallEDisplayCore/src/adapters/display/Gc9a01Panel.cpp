#include "adapters/display/Gc9a01Panel.h"
#include <Arduino.h>

namespace WallE {

Gc9a01Panel::Gc9a01Panel(const Config& config)
    : config_(config),
      asyncWriter_(config.dcPin, config.csPin, config.spiHost,
                   config.spiHz) {}

Gc9a01Panel::~Gc9a01Panel() {
  asyncWriter_.end();
  delete gfx_;
  delete bus_;
}

bool Gc9a01Panel::begin() {
  ready_ = false;
  if (gfx_ != nullptr) {
    ready_ = gfx_->begin(config_.spiHz);
    return ready_;
  }

  if (config_.csPin >= 0) {
    pinMode(config_.csPin, OUTPUT);
    digitalWrite(config_.csPin, HIGH);
  }

  bus_ = new Arduino_ESP32SPIDMA(
      config_.dcPin, config_.csPin, config_.sckPin, config_.mosiPin,
      config_.misoPin, config_.spiHost, true);
  if (bus_ == nullptr) {
    return false;
  }

  gfx_ = new Arduino_GC9A01(bus_, config_.rstPin, config_.rotation, config_.ips,
                            config_.width, config_.height);
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
