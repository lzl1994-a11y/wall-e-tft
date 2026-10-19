#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace WallE {

struct HostNetworkScreen {
  uint32_t sequence = 0;
  char state[11] = {};
  char ip[16] = {};
  uint16_t port = 0;
  char ssid[33] = {};
  char password[64] = {};
};

inline bool parseHostNumber(const char* text, uint32_t maximum, uint32_t& value) {
  value = 0;
  if (!*text) return false;
  for (; *text; ++text) {
    if (*text < '0' || *text > '9') return false;
    const uint32_t digit = static_cast<uint32_t>(*text - '0');
    if (value > maximum / 10 || (value == maximum / 10 && digit > maximum % 10)) return false;
    value = value * 10 + digit;
  }
  return true;
}

// ASCII-only, bounded protocol. No credentials are echoed in ACKs or logs.
inline bool parseHostNetworkScreen(const uint8_t* data, size_t length, HostNetworkScreen& out) {
  constexpr char prefix[] = "hostnet:";
  if (!data || length < sizeof(prefix) || length >= 192 ||
      memcmp(data, prefix, sizeof(prefix) - 1) != 0) return false;
  char buffer[192] = {};
  memcpy(buffer, data + sizeof(prefix) - 1, length - sizeof(prefix) + 1);
  char* fields[7] = {buffer};
  size_t count = 1;
  for (char* cursor = buffer; *cursor; ++cursor) {
    if (*cursor < 32 || *cursor > 126) return false;
    if (*cursor == '|') {
      if (count == 7) return false;
      *cursor = '\0';
      fields[count++] = cursor + 1;
    }
  }
  uint32_t port;
  if (count != 7 || !parseHostNumber(fields[0], UINT32_MAX, out.sequence) ||
      strcmp(fields[1], "1") || !parseHostNumber(fields[4], 65535, port) || port == 0 ||
      strlen(fields[3]) >= sizeof(out.ip) || strlen(fields[5]) >= sizeof(out.ssid) ||
      strlen(fields[6]) >= sizeof(out.password)) return false;
  if (strcmp(fields[2], "setup") && strcmp(fields[2], "connecting") &&
      strcmp(fields[2], "connected") && strcmp(fields[2], "syncing") &&
      strcmp(fields[2], "partial") && strcmp(fields[2], "done")) return false;
  char ip[16];
  strcpy(ip, fields[3]);
  char* start = ip;
  for (int part = 0; part < 4; ++part) {
    char* dot = strchr(start, '.');
    if ((part < 3) != (dot != nullptr)) return false;
    if (dot) *dot = '\0';
    uint32_t octet;
    if (!parseHostNumber(start, 255, octet)) return false;
    if (dot) start = dot + 1;
  }
  strcpy(out.state, fields[2]);
  strcpy(out.ip, fields[3]);
  strcpy(out.ssid, fields[5]);
  strcpy(out.password, fields[6]);
  out.port = static_cast<uint16_t>(port);
  return true;
}

}  // namespace WallE
