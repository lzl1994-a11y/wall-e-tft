#pragma once

#include <stddef.h>
#include <stdint.h>

namespace WallE {

/** Serial-only network configuration boundary. Implementations own Wi-Fi/NVS. */
class INetworkConfigPort {
 public:
  virtual ~INetworkConfigPort() = default;

  // Writes one complete protocol line (without CR/LF). True means the input
  // was a netcfg command and must not be routed to the UI.
  virtual bool handleSerialCommand(const uint8_t* data, size_t length,
                                   char* response, size_t capacity,
                                   bool& applyAccepted) = 0;
  // Called only after the control task has completely emitted the ACK line.
  virtual void acknowledgeApplyOutput() = 0;
  // Retrieves exactly one asynchronous terminal result for serial output.
  virtual bool pollSerialResponse(char* response, size_t capacity) = 0;
};

}  // namespace WallE
