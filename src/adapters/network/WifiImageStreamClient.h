#pragma once

#include "adapters/network/NetworkConfigStore.h"
#include "ports/IImageStreamPort.h"

#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace WallE {

/**
 * Persistent TCP client for length-prefixed JPEG camera frames.
 * Compressed frame slots live in PSRAM; only pointers cross into the UI task.
 */
class WifiImageStreamClient final : public IImageStreamPort {
 public:
  struct Config {
    NetworkConfigData defaultNetworkConfig;
    const char* deviceId = "WALL_E_TFT";
    size_t maxJpegBytes = 256 * 1024;
    uint32_t wifiReconnectMs = 5000;
    uint32_t tcpReconnectMs = 1000;
    uint32_t pingMs = 2000;
    UBaseType_t eventQueueDepth = 8;
    uint32_t taskStackBytes = 8192;
    UBaseType_t taskPriority = 2;
    BaseType_t taskCore = 0;
  };

  explicit WifiImageStreamClient(const Config& config);

  bool begin() override;
  bool poll(ImageStreamEvent& event) override;
  void releaseFrame(uint8_t frameToken) override;
  bool connected() const override { return connected_; }

 private:
  static constexpr size_t kHeaderBytes = 16;
  static constexpr size_t kFrameSlotCount = 2;
  static constexpr size_t kControlPayloadBytes = 512;

  enum class MessageType : uint8_t {
    Hello = 0x01,
    Ping = 0x02,
    Pong = 0x03,
    StreamStart = 0x10,
    JpegFrame = 0x11,
    StreamEnd = 0x12,
    NetworkConfigSet = 0x20,
    NetworkConfigResult = 0x21,
    NetworkConfigApply = 0x22,
    NetworkConfigQuery = 0x23,
    NetworkConfigStatus = 0x24,
  };

  enum class ConfigResult : uint8_t {
    Staged = 0,
    ApplyAccepted = 1,
    TrialConnectedSaved = 2,
    ValidationError = 3,
    NoCandidate = 4,
    StoreError = 5,
    TrialFailedRestored = 6,
  };

  enum class SlotState : uint8_t {
    Free,
    Writing,
    Ready,
    Displaying,
  };

  struct FrameSlot {
    uint8_t* data = nullptr;
    size_t length = 0;
    uint32_t sequence = 0;
    SlotState state = SlotState::Free;
  };

  void run();
  static void taskEntry(void* parameter);
  void serviceWifi();
  void serviceTrial(uint32_t now);
  bool hasConfiguredNetwork() const;
  bool startNextWifiConnection();
  void serviceTcp();
  void processSocketBytes();
  bool startMessage();
  void finishMessage();
  void resetParser();
  int acquireFrameSlot();
  void freeFrameSlot(size_t index);
  bool queueEvent(const ImageStreamEvent& event, TickType_t waitTicks = 0);
  void updateConnected(bool nextConnected);
  bool sendMessage(MessageType type, uint32_t sequence,
                   const uint8_t* payload, size_t payloadLength);
  bool writeAll(const uint8_t* data, size_t length);

  static uint16_t readU16(const uint8_t* data);
  static uint32_t readU32(const uint8_t* data);
  static void writeU16(uint8_t* data, uint16_t value);
  static void writeU32(uint8_t* data, uint32_t value);
  const NetworkConfigData& workingNetworkConfig() const;
  bool parseNetworkConfigSet(NetworkConfigData& destination,
                             uint8_t& validationDetail) const;
  size_t writeNetworkStatus(uint8_t* destination, size_t capacity) const;
  bool sendConfigResult(uint32_t sequence, MessageType operation,
                        ConfigResult result, uint8_t detail = 0);
  void startTrial();
  void finishTrialSuccess();
  void finishTrialFailure(
      ConfigResult result = ConfigResult::TrialFailedRestored);

  Config config_;
  NetworkConfigStore networkConfigStore_;
  NetworkConfigData activeNetworkConfig_;
  NetworkConfigData candidateNetworkConfig_;
  bool candidatePresent_ = false;
  bool trialPending_ = false;
  bool trialInProgress_ = false;
  uint32_t trialStartAtMs_ = 0;
  uint32_t trialDeadlineMs_ = 0;
  uint32_t trialSequence_ = 0;
  bool pendingTrialResult_ = false;
  ConfigResult pendingTrialResultCode_ = ConfigResult::TrialFailedRestored;
  WiFiClient client_;
  QueueHandle_t eventQueue_ = nullptr;
  TaskHandle_t taskHandle_ = nullptr;
  FrameSlot frameSlots_[kFrameSlotCount];
  mutable portMUX_TYPE slotMux_ = portMUX_INITIALIZER_UNLOCKED;

  volatile bool connected_ = false;
  uint32_t lastWifiAttemptMs_ = 0;
  size_t nextWifiNetworkIndex_ = 0;
  uint8_t selectedWifiNetworkIndex_ = 0xFF;
  uint32_t lastTcpAttemptMs_ = 0;
  uint32_t lastPingMs_ = 0;
  uint32_t pingSequence_ = 0;

  uint8_t header_[kHeaderBytes] = {0};
  size_t headerReceived_ = 0;
  MessageType currentType_ = MessageType::Ping;
  uint32_t currentSequence_ = 0;
  size_t currentPayloadLength_ = 0;
  size_t currentPayloadReceived_ = 0;
  uint8_t controlPayload_[kControlPayloadBytes] = {0};
  int writingSlot_ = -1;
  bool discardPayload_ = false;
};

}  // namespace WallE
