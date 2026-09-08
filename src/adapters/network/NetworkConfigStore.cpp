#include "adapters/network/NetworkConfigStore.h"

#include <ctype.h>
#include <string.h>

namespace WallE {

namespace {
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
  // Strict UTF-8 validation keeps byte limits unambiguous and rejects broken
  // serial input before it can reach WiFi.begin().
  for (size_t index = 0; index < length;) {
    const uint8_t first = static_cast<uint8_t>(value[index]);
    size_t extra = 0;
    uint32_t codepoint = 0;
    if (first < 0x80) { ++index; continue; }
    if (first >= 0xC2 && first <= 0xDF) { extra = 1; codepoint = first & 0x1F; }
    else if (first >= 0xE0 && first <= 0xEF) { extra = 2; codepoint = first & 0x0F; }
    else if (first >= 0xF0 && first <= 0xF4) { extra = 3; codepoint = first & 0x07; }
    else return false;
    if (index + extra >= length) return false;
    for (size_t next = 1; next <= extra; ++next) {
      const uint8_t byte = static_cast<uint8_t>(value[index + next]);
      if ((byte & 0xC0) != 0x80) return false;
      codepoint = (codepoint << 6) | (byte & 0x3F);
    }
    if ((extra == 2 && codepoint < 0x800) ||
        (extra == 3 && codepoint < 0x10000) ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint > 0x10FFFF) return false;
    index += extra + 1;
  }
  return true;
}
}  // namespace

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

}  // namespace WallE
