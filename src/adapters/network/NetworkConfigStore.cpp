#include "adapters/network/NetworkConfigStore.h"

#include <Preferences.h>
#include <ctype.h>
#include <string.h>

namespace WallE {

namespace {
constexpr char kNvsNamespace[] = "walle_net";
constexpr char kActiveKey[] = "active_v1";

bool copyText(char* destination, size_t capacity, const char* source) {
  if (destination == nullptr || capacity == 0 || source == nullptr) {
    return source == nullptr;
  }
  const size_t length = strnlen(source, capacity);
  if (length >= capacity) {
    return false;
  }
  memcpy(destination, source, length);
  destination[length] = '\0';
  return true;
}

bool validText(const char* value, size_t maximum, bool required) {
  if (value == nullptr) {
    return false;
  }
  const size_t length = strnlen(value, maximum + 1);
  if (length > maximum || (required && length == 0)) {
    return false;
  }
  for (size_t index = 0; index < length; ++index) {
    const unsigned char byte = static_cast<unsigned char>(value[index]);
    if (byte < 0x20 || byte == 0x7f) {
      return false;
    }
  }
  return true;
}
}  // namespace

bool NetworkConfigData::setDefaults(const char* ssid1, const char* password1,
                                    const char* ssid2, const char* password2,
                                    const char* ssid3, const char* password3,
                                    const char* imageHost,
                                    uint16_t imagePort) {
  const char* ssids[kWifiCount] = {ssid1, ssid2, ssid3};
  const char* passwords[kWifiCount] = {password1, password2, password3};
  NetworkConfigData next;
  next.version = kVersion;
  next.port = imagePort;
  if (!copyText(next.host, sizeof(next.host), imageHost)) {
    return false;
  }
  for (size_t index = 0; index < kWifiCount; ++index) {
    if (!copyText(next.wifi[index].ssid, sizeof(next.wifi[index].ssid),
                  ssids[index]) ||
        !copyText(next.wifi[index].password,
                  sizeof(next.wifi[index].password), passwords[index])) {
      return false;
    }
  }
  if (!next.valid()) {
    return false;
  }
  *this = next;
  return true;
}

bool NetworkConfigData::hasWifi() const {
  for (const WifiCredential& item : wifi) {
    if (item.ssid[0] != '\0') {
      return true;
    }
  }
  return false;
}

bool NetworkConfigData::valid() const {
  if (version != kVersion || !hasWifi() || !validText(host, kMaxHostBytes, true) ||
      port == 0) {
    return false;
  }
  for (const WifiCredential& item : wifi) {
    if (!validText(item.ssid, kMaxSsidBytes, false) ||
        !validText(item.password, kMaxPasswordBytes, false)) {
      return false;
    }
  }
  return true;
}

bool NetworkConfigStore::load(const NetworkConfigData& defaults) {
  active_ = NetworkConfigData{};
  activeFromNvs_ = false;

  Preferences preferences;
  if (preferences.begin(kNvsNamespace, true)) {
    NetworkConfigData saved;
    const size_t size = preferences.getBytesLength(kActiveKey);
    const size_t read = size == sizeof(saved)
                            ? preferences.getBytes(kActiveKey, &saved,
                                                   sizeof(saved))
                            : 0;
    preferences.end();
    if (read == sizeof(saved) && saved.valid()) {
      active_ = saved;
      activeFromNvs_ = true;
      return true;
    }
  }

  if (!defaults.valid()) {
    return false;
  }
  active_ = defaults;
  return true;
}

bool NetworkConfigStore::saveActive(const NetworkConfigData& config) {
  if (!config.valid()) {
    return false;
  }
  Preferences preferences;
  if (!preferences.begin(kNvsNamespace, false)) {
    return false;
  }
  const size_t written = preferences.putBytes(kActiveKey, &config, sizeof(config));
  preferences.end();
  if (written != sizeof(config)) {
    return false;
  }
  active_ = config;
  activeFromNvs_ = true;
  return true;
}

}  // namespace WallE
