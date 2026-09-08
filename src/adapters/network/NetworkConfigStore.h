#pragma once

#include <stddef.h>
#include <stdint.h>

namespace WallE {

/**
 * Fixed-size runtime network data. Keeping credentials in bounded buffers
 * makes it possible to validate serial packets before touching Wi-Fi.
 */
struct NetworkConfigData {
  static constexpr uint8_t kVersion = 2;
  static constexpr size_t kWifiCount = 3;
  static constexpr size_t kMaxSsidBytes = 32;
  static constexpr size_t kMaxPasswordBytes = 64;
  static constexpr size_t kMaxHostBytes = 64;

  struct WifiCredential {
    char ssid[kMaxSsidBytes + 1] = {0};
    char password[kMaxPasswordBytes + 1] = {0};
  };

  uint8_t version = kVersion;
  WifiCredential wifi[kWifiCount];
  char host[kMaxHostBytes + 1] = {0};
  uint16_t port = 0;

  bool valid() const;
  bool hasWifi() const;
};

}  // namespace WallE
