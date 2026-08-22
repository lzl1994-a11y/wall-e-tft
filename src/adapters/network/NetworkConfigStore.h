#pragma once

#include <stddef.h>
#include <stdint.h>

namespace WallE {

/**
 * Fixed-size runtime network data. Keeping credentials in bounded buffers
 * makes it possible to validate network packets before touching Wi-Fi/NVS.
 */
struct NetworkConfigData {
  static constexpr uint8_t kVersion = 1;
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

  bool setDefaults(const char* ssid1, const char* password1,
                   const char* ssid2, const char* password2,
                   const char* ssid3, const char* password3,
                   const char* imageHost, uint16_t imagePort);
  bool valid() const;
  bool hasWifi() const;
};

/** Active configuration persistence. Candidates deliberately never enter NVS. */
class NetworkConfigStore {
 public:
  bool load(const NetworkConfigData& defaults);
  bool saveActive(const NetworkConfigData& config);

  const NetworkConfigData& active() const { return active_; }
  bool activeFromNvs() const { return activeFromNvs_; }

 private:
  NetworkConfigData active_;
  bool activeFromNvs_ = false;
};

}  // namespace WallE
