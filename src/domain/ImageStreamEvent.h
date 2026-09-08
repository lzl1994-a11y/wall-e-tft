#pragma once

#include <stddef.h>
#include <stdint.h>

namespace WallE {

enum class ImageStreamEventType : uint8_t {
  Connected,
  Disconnected,
  StreamStarted,
  FrameReady,
  StreamEnded,
  ProtocolError,
  NetworkConfigConnected,
  NetworkConfigFailed,
};

/** Event delivered by the background Wi-Fi image-stream client. */
struct ImageStreamEvent {
  ImageStreamEventType type = ImageStreamEventType::Disconnected;
  const uint8_t* data = nullptr;
  size_t length = 0;
  uint32_t sequence = 0;
  uint32_t streamDurationMs = 0;
  uint32_t holdDurationMs = 0;
  uint16_t targetFps = 0;
  uint8_t frameToken = 0xFF;
  char host[65] = {0};
  uint16_t port = 0;
};

}  // namespace WallE
