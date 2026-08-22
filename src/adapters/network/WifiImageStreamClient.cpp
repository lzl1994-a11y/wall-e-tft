#include "adapters/network/WifiImageStreamClient.h"

#include <esp_heap_caps.h>
#include <string.h>

namespace WallE {

namespace {
constexpr uint8_t kProtocolMagic[] = {'W', 'T', 'F', 'T'};
constexpr uint8_t kProtocolVersion = 1;
constexpr uint8_t kNetworkPayloadVersion = 1;
constexpr TickType_t kFrameEventQueueWait = pdMS_TO_TICKS(20);
constexpr TickType_t kFrameSlotWait = pdMS_TO_TICKS(250);
constexpr uint32_t kNetworkTrialDelayMs = 500;
constexpr uint32_t kNetworkTrialTimeoutMs = 60000;

enum class ConfigValidationDetail : uint8_t {
  None = 0,
  Version = 1,
  WifiCount = 2,
  SsidLength = 3,
  PasswordLength = 4,
  HostLength = 5,
  Port = 6,
  Length = 7,
  NoWifi = 8,
};

bool validWireText(const uint8_t* data, size_t length, bool required) {
  if (data == nullptr || (required && length == 0)) {
    return false;
  }
  for (size_t index = 0; index < length; ++index) {
    if (data[index] < 0x20 || data[index] == 0x7f) {
      return false;
    }
  }
  return true;
}
}  // namespace

WifiImageStreamClient::WifiImageStreamClient(const Config& config)
    : config_(config) {}

bool WifiImageStreamClient::begin() {
  if (taskHandle_ != nullptr) {
    return true;
  }
  if (!networkConfigStore_.load(config_.defaultNetworkConfig)) {
    return false;
  }
  activeNetworkConfig_ = networkConfigStore_.active();
  if (!hasConfiguredNetwork() || config_.maxJpegBytes == 0 || !psramFound()) {
    return false;
  }

  eventQueue_ = xQueueCreate(config_.eventQueueDepth,
                             sizeof(ImageStreamEvent));
  if (eventQueue_ == nullptr) {
    return false;
  }

  for (size_t index = 0; index < kFrameSlotCount; ++index) {
    frameSlots_[index].data = static_cast<uint8_t*>(heap_caps_malloc(
        config_.maxJpegBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (frameSlots_[index].data == nullptr) {
      for (size_t cleanup = 0; cleanup < index; ++cleanup) {
        heap_caps_free(frameSlots_[cleanup].data);
        frameSlots_[cleanup].data = nullptr;
      }
      vQueueDelete(eventQueue_);
      eventQueue_ = nullptr;
      return false;
    }
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  // We deliberately own reconnects so failed credentials rotate instead of
  // repeatedly retrying only the last access point.
  WiFi.setAutoReconnect(false);
  if (!startNextWifiConnection()) {
    vQueueDelete(eventQueue_);
    eventQueue_ = nullptr;
    for (FrameSlot& slot : frameSlots_) {
      heap_caps_free(slot.data);
      slot.data = nullptr;
    }
    return false;
  }

  const BaseType_t created = xTaskCreatePinnedToCore(
      taskEntry, "walle_image_net", config_.taskStackBytes, this,
      config_.taskPriority, &taskHandle_, config_.taskCore);
  if (created != pdPASS) {
    taskHandle_ = nullptr;
    for (FrameSlot& slot : frameSlots_) {
      heap_caps_free(slot.data);
      slot.data = nullptr;
    }
    vQueueDelete(eventQueue_);
    eventQueue_ = nullptr;
    return false;
  }
  return true;
}

bool WifiImageStreamClient::poll(ImageStreamEvent& event) {
  if (eventQueue_ == nullptr) {
    return false;
  }

  ImageStreamEvent next;
  while (xQueueReceive(eventQueue_, &next, 0) == pdTRUE) {
    if (next.type != ImageStreamEventType::FrameReady) {
      event = next;
      return true;
    }
    if (next.frameToken >= kFrameSlotCount) {
      continue;
    }

    bool valid = false;
    portENTER_CRITICAL(&slotMux_);
    FrameSlot& slot = frameSlots_[next.frameToken];
    if (slot.state == SlotState::Ready &&
        slot.sequence == next.sequence) {
      slot.state = SlotState::Displaying;
      next.data = slot.data;
      next.length = slot.length;
      valid = true;
    }
    portEXIT_CRITICAL(&slotMux_);
    if (valid) {
      event = next;
      return true;
    }
  }
  return false;
}

void WifiImageStreamClient::releaseFrame(uint8_t frameToken) {
  if (frameToken >= kFrameSlotCount) {
    return;
  }
  portENTER_CRITICAL(&slotMux_);
  FrameSlot& slot = frameSlots_[frameToken];
  if (slot.state == SlotState::Displaying) {
    slot.state = SlotState::Free;
    slot.length = 0;
  }
  portEXIT_CRITICAL(&slotMux_);
}

void WifiImageStreamClient::taskEntry(void* parameter) {
  WifiImageStreamClient* self =
      static_cast<WifiImageStreamClient*>(parameter);
  if (self != nullptr) {
    self->run();
  }
  vTaskDelete(nullptr);
}

void WifiImageStreamClient::run() {
  for (;;) {
    serviceWifi();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void WifiImageStreamClient::serviceWifi() {
  const uint32_t now = millis();
  serviceTrial(now);
  if (WiFi.status() != WL_CONNECTED) {
    if (client_.connected() || connected_) {
      client_.stop();
      resetParser();
      updateConnected(false);
    }
    if (now - lastWifiAttemptMs_ >= config_.wifiReconnectMs) {
      startNextWifiConnection();
    }
    return;
  }
  serviceTcp();
}

void WifiImageStreamClient::serviceTrial(uint32_t now) {
  if (trialPending_ && !trialInProgress_ &&
      static_cast<int32_t>(now - trialStartAtMs_) >= 0) {
    startTrial();
  }
  if (trialInProgress_ && static_cast<int32_t>(now - trialDeadlineMs_) >= 0) {
    finishTrialFailure();
  }
}

bool WifiImageStreamClient::hasConfiguredNetwork() const {
  return workingNetworkConfig().valid();
}

bool WifiImageStreamClient::startNextWifiConnection() {
  const NetworkConfigData& networkConfig = workingNetworkConfig();
  if (!networkConfig.valid()) {
    return false;
  }

  for (size_t offset = 0; offset < NetworkConfigData::kWifiCount; ++offset) {
    const size_t index =
        (nextWifiNetworkIndex_ + offset) % NetworkConfigData::kWifiCount;
    const NetworkConfigData::WifiCredential& network = networkConfig.wifi[index];
    if (network.ssid[0] == '\0') {
      continue;
    }

    nextWifiNetworkIndex_ = (index + 1) % NetworkConfigData::kWifiCount;
    selectedWifiNetworkIndex_ = static_cast<uint8_t>(index);
    lastWifiAttemptMs_ = millis();
    WiFi.disconnect(false, false);
    WiFi.begin(network.ssid, network.password);
    return true;
  }
  return false;
}

void WifiImageStreamClient::serviceTcp() {
  const uint32_t now = millis();
  if (!client_.connected()) {
    if (connected_) {
      client_.stop();
      resetParser();
      updateConnected(false);
    }
    if (now - lastTcpAttemptMs_ < config_.tcpReconnectMs) {
      return;
    }
    lastTcpAttemptMs_ = now;
    const NetworkConfigData& networkConfig = workingNetworkConfig();
    if (!client_.connect(networkConfig.host, networkConfig.port, 500)) {
      client_.stop();
      return;
    }
    client_.setNoDelay(true);
    resetParser();
    updateConnected(true);
    const uint8_t* id = reinterpret_cast<const uint8_t*>(config_.deviceId);
    const size_t idLength = config_.deviceId == nullptr
                                ? 0
                                : strlen(config_.deviceId);
    if (!sendMessage(MessageType::Hello, 0, id, idLength)) {
      client_.stop();
      updateConnected(false);
      return;
    }
    if (trialInProgress_) {
      finishTrialSuccess();
    } else if (pendingTrialResult_) {
      if (sendConfigResult(trialSequence_, MessageType::NetworkConfigApply,
                           pendingTrialResultCode_)) {
        pendingTrialResult_ = false;
      } else {
        client_.stop();
        resetParser();
        updateConnected(false);
        return;
      }
    }
    lastPingMs_ = now;
  }

  processSocketBytes();
  if (!client_.connected()) {
    client_.stop();
    resetParser();
    updateConnected(false);
    return;
  }

  if (now - lastPingMs_ >= config_.pingMs) {
    lastPingMs_ = now;
    sendMessage(MessageType::Ping, ++pingSequence_, nullptr, 0);
  }
}

void WifiImageStreamClient::processSocketBytes() {
  uint8_t discard[256];
  while (client_.connected() && client_.available() > 0) {
    if (headerReceived_ < kHeaderBytes) {
      const size_t wanted = kHeaderBytes - headerReceived_;
      const int received = client_.read(header_ + headerReceived_, wanted);
      if (received <= 0) {
        return;
      }
      headerReceived_ += static_cast<size_t>(received);
      if (headerReceived_ < kHeaderBytes) {
        continue;
      }
      if (!startMessage()) {
        ImageStreamEvent error;
        error.type = ImageStreamEventType::ProtocolError;
        queueEvent(error);
        client_.stop();
        resetParser();
        return;
      }
      if (currentPayloadLength_ == 0) {
        finishMessage();
        resetParser();
        continue;
      }
    }

    const size_t remaining = currentPayloadLength_ - currentPayloadReceived_;
    const size_t available = static_cast<size_t>(client_.available());
    if (remaining == 0 || available == 0) {
      return;
    }
    size_t wanted = min(remaining, available);
    uint8_t* destination = nullptr;
    if (!discardPayload_ && currentType_ == MessageType::JpegFrame &&
        writingSlot_ >= 0) {
      destination = frameSlots_[writingSlot_].data + currentPayloadReceived_;
    } else if (!discardPayload_) {
      destination = controlPayload_ + currentPayloadReceived_;
    } else {
      wanted = min(wanted, sizeof(discard));
      destination = discard;
    }

    const int received = client_.read(destination, wanted);
    if (received <= 0) {
      return;
    }
    currentPayloadReceived_ += static_cast<size_t>(received);
    if (currentPayloadReceived_ == currentPayloadLength_) {
      finishMessage();
      resetParser();
    }
  }
}

bool WifiImageStreamClient::startMessage() {
  if (memcmp(header_, kProtocolMagic, sizeof(kProtocolMagic)) != 0 ||
      header_[4] != kProtocolVersion) {
    return false;
  }

  currentType_ = static_cast<MessageType>(header_[5]);
  currentSequence_ = readU32(header_ + 8);
  currentPayloadLength_ = readU32(header_ + 12);
  currentPayloadReceived_ = 0;
  writingSlot_ = -1;
  discardPayload_ = false;

  switch (currentType_) {
    case MessageType::Hello:
    case MessageType::Ping:
    case MessageType::Pong:
    case MessageType::StreamStart:
    case MessageType::StreamEnd:
    case MessageType::NetworkConfigSet:
    case MessageType::NetworkConfigApply:
    case MessageType::NetworkConfigQuery:
      if (currentPayloadLength_ > sizeof(controlPayload_)) {
        discardPayload_ = true;
      }
      break;
    case MessageType::JpegFrame:
      if (currentPayloadLength_ == 0 ||
          currentPayloadLength_ > config_.maxJpegBytes) {
        discardPayload_ = true;
        ImageStreamEvent error;
        error.type = ImageStreamEventType::ProtocolError;
        error.sequence = currentSequence_;
        queueEvent(error);
      } else {
        writingSlot_ = acquireFrameSlot();
        discardPayload_ = writingSlot_ < 0;
      }
      break;
    default:
      return false;
  }
  return true;
}

void WifiImageStreamClient::finishMessage() {
  switch (currentType_) {
    case MessageType::StreamStart: {
      ImageStreamEvent event;
      event.type = ImageStreamEventType::StreamStarted;
      event.sequence = currentSequence_;
      if (!discardPayload_ && currentPayloadLength_ >= 10) {
        event.streamDurationMs = readU32(controlPayload_);
        event.holdDurationMs = readU32(controlPayload_ + 4);
        event.targetFps = readU16(controlPayload_ + 8);
      }
      queueEvent(event);
      break;
    }
    case MessageType::JpegFrame: {
      if (writingSlot_ < 0 || discardPayload_) {
        break;
      }
      FrameSlot& slot = frameSlots_[writingSlot_];
      const bool validJpeg = currentPayloadLength_ >= 4 &&
                             slot.data[0] == 0xFF && slot.data[1] == 0xD8 &&
                             slot.data[currentPayloadLength_ - 2] == 0xFF &&
                             slot.data[currentPayloadLength_ - 1] == 0xD9;
      if (!validJpeg) {
        freeFrameSlot(static_cast<size_t>(writingSlot_));
        ImageStreamEvent error;
        error.type = ImageStreamEventType::ProtocolError;
        error.sequence = currentSequence_;
        queueEvent(error);
        break;
      }

      portENTER_CRITICAL(&slotMux_);
      slot.length = currentPayloadLength_;
      slot.sequence = currentSequence_;
      slot.state = SlotState::Ready;
      portEXIT_CRITICAL(&slotMux_);

      ImageStreamEvent event;
      event.type = ImageStreamEventType::FrameReady;
      event.sequence = currentSequence_;
      event.length = currentPayloadLength_;
      event.frameToken = static_cast<uint8_t>(writingSlot_);
      if (!queueEvent(event, kFrameEventQueueWait)) {
        freeFrameSlot(static_cast<size_t>(writingSlot_));
      }
      writingSlot_ = -1;
      break;
    }
    case MessageType::StreamEnd: {
      ImageStreamEvent event;
      event.type = ImageStreamEventType::StreamEnded;
      event.sequence = currentSequence_;
      queueEvent(event, kFrameEventQueueWait);
      break;
    }
    case MessageType::NetworkConfigSet: {
      NetworkConfigData parsed;
      uint8_t detail = static_cast<uint8_t>(ConfigValidationDetail::None);
      if (trialInProgress_ || trialPending_) {
        sendConfigResult(currentSequence_, MessageType::NetworkConfigSet,
                         ConfigResult::ValidationError,
                         static_cast<uint8_t>(ConfigValidationDetail::Length));
      } else if (discardPayload_ ||
          !parseNetworkConfigSet(parsed, detail)) {
        sendConfigResult(currentSequence_, MessageType::NetworkConfigSet,
                         ConfigResult::ValidationError, detail);
      } else {
        candidateNetworkConfig_ = parsed;
        candidatePresent_ = true;
        sendConfigResult(currentSequence_, MessageType::NetworkConfigSet,
                         ConfigResult::Staged);
      }
      break;
    }
    case MessageType::NetworkConfigApply:
      if (discardPayload_ || currentPayloadLength_ != 1 ||
          controlPayload_[0] != kNetworkPayloadVersion) {
        sendConfigResult(currentSequence_, MessageType::NetworkConfigApply,
                         ConfigResult::ValidationError,
                         static_cast<uint8_t>(ConfigValidationDetail::Version));
      } else if (!candidatePresent_ || !candidateNetworkConfig_.valid()) {
        sendConfigResult(currentSequence_, MessageType::NetworkConfigApply,
                         ConfigResult::NoCandidate);
      } else if (trialInProgress_ || trialPending_) {
        sendConfigResult(currentSequence_, MessageType::NetworkConfigApply,
                         ConfigResult::ValidationError,
                         static_cast<uint8_t>(ConfigValidationDetail::Length));
      } else {
        // ACK is fully sent while the old connection is still alive. The task
        // starts the disruptive Wi-Fi/TCP trial only after the 500 ms grace.
        if (sendConfigResult(currentSequence_, MessageType::NetworkConfigApply,
                             ConfigResult::ApplyAccepted)) {
          trialSequence_ = currentSequence_;
          trialStartAtMs_ = millis() + kNetworkTrialDelayMs;
          trialPending_ = true;
        }
      }
      break;
    case MessageType::NetworkConfigQuery: {
      if (discardPayload_ || currentPayloadLength_ != 1 ||
          controlPayload_[0] != kNetworkPayloadVersion) {
        sendConfigResult(currentSequence_, MessageType::NetworkConfigQuery,
                         ConfigResult::ValidationError,
                         static_cast<uint8_t>(ConfigValidationDetail::Version));
        break;
      }
      uint8_t payload[kControlPayloadBytes] = {0};
      const size_t length = writeNetworkStatus(payload, sizeof(payload));
      if (length > 0) {
        sendMessage(MessageType::NetworkConfigStatus, currentSequence_, payload,
                    length);
      }
      break;
    }
    case MessageType::Ping:
      sendMessage(MessageType::Pong, currentSequence_, nullptr, 0);
      break;
    case MessageType::Hello:
    case MessageType::Pong:
    case MessageType::NetworkConfigResult:
    case MessageType::NetworkConfigStatus:
      break;
  }
}

const NetworkConfigData& WifiImageStreamClient::workingNetworkConfig() const {
  return trialInProgress_ ? candidateNetworkConfig_ : activeNetworkConfig_;
}

bool WifiImageStreamClient::parseNetworkConfigSet(
    NetworkConfigData& destination, uint8_t& validationDetail) const {
  validationDetail = static_cast<uint8_t>(ConfigValidationDetail::None);
  if (currentPayloadLength_ < 2 || controlPayload_[0] != kNetworkPayloadVersion) {
    validationDetail = static_cast<uint8_t>(ConfigValidationDetail::Version);
    return false;
  }
  if (controlPayload_[1] != NetworkConfigData::kWifiCount) {
    validationDetail = static_cast<uint8_t>(ConfigValidationDetail::WifiCount);
    return false;
  }

  NetworkConfigData parsed;
  parsed.version = NetworkConfigData::kVersion;
  size_t offset = 2;
  for (size_t index = 0; index < NetworkConfigData::kWifiCount; ++index) {
    if (offset >= currentPayloadLength_) {
      validationDetail = static_cast<uint8_t>(ConfigValidationDetail::Length);
      return false;
    }
    const size_t ssidLength = controlPayload_[offset++];
    if (ssidLength > NetworkConfigData::kMaxSsidBytes ||
        offset + ssidLength >= currentPayloadLength_ ||
        !validWireText(controlPayload_ + offset, ssidLength, false)) {
      validationDetail = static_cast<uint8_t>(ConfigValidationDetail::SsidLength);
      return false;
    }
    memcpy(parsed.wifi[index].ssid, controlPayload_ + offset, ssidLength);
    parsed.wifi[index].ssid[ssidLength] = '\0';
    offset += ssidLength;

    const size_t passwordLength = controlPayload_[offset++];
    if (passwordLength > NetworkConfigData::kMaxPasswordBytes ||
        offset + passwordLength > currentPayloadLength_ ||
        !validWireText(controlPayload_ + offset, passwordLength, false)) {
      validationDetail = static_cast<uint8_t>(ConfigValidationDetail::PasswordLength);
      return false;
    }
    memcpy(parsed.wifi[index].password, controlPayload_ + offset,
           passwordLength);
    parsed.wifi[index].password[passwordLength] = '\0';
    offset += passwordLength;
  }
  if (offset >= currentPayloadLength_) {
    validationDetail = static_cast<uint8_t>(ConfigValidationDetail::Length);
    return false;
  }
  const size_t hostLength = controlPayload_[offset++];
  if (hostLength == 0 || hostLength > NetworkConfigData::kMaxHostBytes ||
      offset + hostLength + 2 != currentPayloadLength_ ||
      !validWireText(controlPayload_ + offset, hostLength, true)) {
    validationDetail = static_cast<uint8_t>(ConfigValidationDetail::HostLength);
    return false;
  }
  memcpy(parsed.host, controlPayload_ + offset, hostLength);
  parsed.host[hostLength] = '\0';
  offset += hostLength;
  parsed.port = readU16(controlPayload_ + offset);
  if (parsed.port == 0) {
    validationDetail = static_cast<uint8_t>(ConfigValidationDetail::Port);
    return false;
  }
  if (!parsed.valid()) {
    validationDetail = static_cast<uint8_t>(ConfigValidationDetail::NoWifi);
    return false;
  }
  destination = parsed;
  return true;
}

size_t WifiImageStreamClient::writeNetworkStatus(uint8_t* destination,
                                                  size_t capacity) const {
  if (destination == nullptr || capacity < 6) {
    return 0;
  }
  // Never serialize credentials here. Status intentionally exposes SSIDs only.
  size_t offset = 0;
  destination[offset++] = kNetworkPayloadVersion;
  uint8_t flags = networkConfigStore_.activeFromNvs() ? 0x01 : 0x00;
  if (candidatePresent_) {
    flags |= 0x02;
  }
  if (trialInProgress_ || trialPending_) {
    flags |= 0x04;
  }
  destination[offset++] = flags;
  destination[offset++] = selectedWifiNetworkIndex_;
  const NetworkConfigData& active = activeNetworkConfig_;
  for (const NetworkConfigData::WifiCredential& network : active.wifi) {
    const size_t length = strnlen(network.ssid, sizeof(network.ssid));
    if (length > UINT8_MAX || offset + 1 + length > capacity) {
      return 0;
    }
    destination[offset++] = static_cast<uint8_t>(length);
    memcpy(destination + offset, network.ssid, length);
    offset += length;
  }
  const size_t hostLength = strnlen(active.host, sizeof(active.host));
  if (hostLength == 0 || hostLength > UINT8_MAX ||
      offset + 1 + hostLength + 2 > capacity) {
    return 0;
  }
  destination[offset++] = static_cast<uint8_t>(hostLength);
  memcpy(destination + offset, active.host, hostLength);
  offset += hostLength;
  writeU16(destination + offset, active.port);
  return offset + 2;
}

bool WifiImageStreamClient::sendConfigResult(uint32_t sequence,
                                             MessageType operation,
                                             ConfigResult result,
                                             uint8_t detail) {
  const uint8_t payload[] = {kNetworkPayloadVersion,
                             static_cast<uint8_t>(operation),
                             static_cast<uint8_t>(result), detail};
  return sendMessage(MessageType::NetworkConfigResult, sequence, payload,
                     sizeof(payload));
}

void WifiImageStreamClient::startTrial() {
  trialPending_ = false;
  trialStartAtMs_ = 0;
  if (!candidatePresent_ || !candidateNetworkConfig_.valid()) {
    if (!sendConfigResult(trialSequence_, MessageType::NetworkConfigApply,
                          ConfigResult::NoCandidate)) {
      pendingTrialResult_ = true;
      pendingTrialResultCode_ = ConfigResult::NoCandidate;
    }
    return;
  }
  trialInProgress_ = true;
  trialDeadlineMs_ = millis() + kNetworkTrialTimeoutMs;
  nextWifiNetworkIndex_ = 0;
  selectedWifiNetworkIndex_ = 0xFF;
  client_.stop();
  resetParser();
  updateConnected(false);
  WiFi.disconnect(false, false);
  lastWifiAttemptMs_ = 0;
}

void WifiImageStreamClient::finishTrialSuccess() {
  if (!networkConfigStore_.saveActive(candidateNetworkConfig_)) {
    finishTrialFailure(ConfigResult::StoreError);
    return;
  }
  activeNetworkConfig_ = networkConfigStore_.active();
  trialInProgress_ = false;
  trialDeadlineMs_ = 0;
  candidatePresent_ = false;
  if (!sendConfigResult(trialSequence_, MessageType::NetworkConfigApply,
                        ConfigResult::TrialConnectedSaved)) {
    pendingTrialResult_ = true;
    pendingTrialResultCode_ = ConfigResult::TrialConnectedSaved;
    client_.stop();
    resetParser();
    updateConnected(false);
  }
}

void WifiImageStreamClient::finishTrialFailure(ConfigResult result) {
  if (!trialInProgress_ && !trialPending_) {
    return;
  }
  trialPending_ = false;
  trialInProgress_ = false;
  trialStartAtMs_ = 0;
  trialDeadlineMs_ = 0;
  nextWifiNetworkIndex_ = 0;
  selectedWifiNetworkIndex_ = 0xFF;
  client_.stop();
  resetParser();
  updateConnected(false);
  WiFi.disconnect(false, false);
  lastWifiAttemptMs_ = 0;
  pendingTrialResult_ = true;
  pendingTrialResultCode_ = result;
}

void WifiImageStreamClient::resetParser() {
  if (writingSlot_ >= 0) {
    freeFrameSlot(static_cast<size_t>(writingSlot_));
  }
  headerReceived_ = 0;
  currentPayloadLength_ = 0;
  currentPayloadReceived_ = 0;
  writingSlot_ = -1;
  discardPayload_ = false;
  memset(header_, 0, sizeof(header_));
  memset(controlPayload_, 0, sizeof(controlPayload_));
}

int WifiImageStreamClient::acquireFrameSlot() {
  const TickType_t startedAt = xTaskGetTickCount();
  do {
    int selected = -1;
    portENTER_CRITICAL(&slotMux_);
    for (size_t index = 0; index < kFrameSlotCount; ++index) {
      if (frameSlots_[index].state == SlotState::Free) {
        frameSlots_[index].state = SlotState::Writing;
        frameSlots_[index].length = 0;
        selected = static_cast<int>(index);
        break;
      }
    }
    portEXIT_CRITICAL(&slotMux_);
    if (selected >= 0) {
      return selected;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  } while (xTaskGetTickCount() - startedAt < kFrameSlotWait);
  return -1;
}

void WifiImageStreamClient::freeFrameSlot(size_t index) {
  if (index >= kFrameSlotCount) {
    return;
  }
  portENTER_CRITICAL(&slotMux_);
  frameSlots_[index].state = SlotState::Free;
  frameSlots_[index].length = 0;
  portEXIT_CRITICAL(&slotMux_);
}

bool WifiImageStreamClient::queueEvent(const ImageStreamEvent& event,
                                       TickType_t waitTicks) {
  return eventQueue_ != nullptr &&
         xQueueSend(eventQueue_, &event, waitTicks) == pdTRUE;
}

void WifiImageStreamClient::updateConnected(bool nextConnected) {
  if (connected_ == nextConnected) {
    return;
  }
  connected_ = nextConnected;
  ImageStreamEvent event;
  event.type = nextConnected ? ImageStreamEventType::Connected
                             : ImageStreamEventType::Disconnected;
  queueEvent(event);
}

bool WifiImageStreamClient::sendMessage(MessageType type, uint32_t sequence,
                                        const uint8_t* payload,
                                        size_t payloadLength) {
  if (!client_.connected() || payloadLength > UINT32_MAX) {
    return false;
  }
  uint8_t header[kHeaderBytes] = {0};
  memcpy(header, kProtocolMagic, sizeof(kProtocolMagic));
  header[4] = kProtocolVersion;
  header[5] = static_cast<uint8_t>(type);
  writeU16(header + 6, 0);
  writeU32(header + 8, sequence);
  writeU32(header + 12, static_cast<uint32_t>(payloadLength));
  if (!writeAll(header, sizeof(header))) {
    return false;
  }
  return payloadLength == 0 || writeAll(payload, payloadLength);
}

bool WifiImageStreamClient::writeAll(const uint8_t* data, size_t length) {
  if (data == nullptr && length > 0) {
    return false;
  }
  size_t written = 0;
  const uint32_t startedAt = millis();
  while (written < length && client_.connected()) {
    const size_t count = client_.write(data + written, length - written);
    if (count > 0) {
      written += count;
      continue;
    }
    if (millis() - startedAt > 1000) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  return written == length;
}

uint16_t WifiImageStreamClient::readU16(const uint8_t* data) {
  return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) |
                               data[1]);
}

uint32_t WifiImageStreamClient::readU32(const uint8_t* data) {
  return (static_cast<uint32_t>(data[0]) << 24) |
         (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) |
         static_cast<uint32_t>(data[3]);
}

void WifiImageStreamClient::writeU16(uint8_t* data, uint16_t value) {
  data[0] = static_cast<uint8_t>(value >> 8);
  data[1] = static_cast<uint8_t>(value);
}

void WifiImageStreamClient::writeU32(uint8_t* data, uint32_t value) {
  data[0] = static_cast<uint8_t>(value >> 24);
  data[1] = static_cast<uint8_t>(value >> 16);
  data[2] = static_cast<uint8_t>(value >> 8);
  data[3] = static_cast<uint8_t>(value);
}

}  // namespace WallE
