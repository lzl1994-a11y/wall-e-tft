#include "adapters/network/WifiImageStreamClient.h"

#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

namespace WallE {

namespace {
constexpr uint8_t kProtocolMagic[] = {'W', 'T', 'F', 'T'};
constexpr uint8_t kProtocolVersion = 1;
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

struct SerialResult { uint32_t sequence; uint8_t result; uint8_t detail; };

bool reached(uint32_t now, uint32_t target) {
  return static_cast<int32_t>(now - target) >= 0;
}

bool asciiEqualsIgnoreCase(const uint8_t* value, size_t length,
                           const char* expected) {
  size_t index = 0;
  for (; index < length && expected[index] != '\0'; ++index) {
    char actual = static_cast<char>(value[index]);
    char wanted = expected[index];
    if (actual >= 'A' && actual <= 'Z') actual = actual - 'A' + 'a';
    if (wanted >= 'A' && wanted <= 'Z') wanted = wanted - 'A' + 'a';
    if (actual != wanted) return false;
  }
  return index == length && expected[index] == '\0';
}

bool parseU32(const uint8_t* value, size_t length, uint32_t& result) {
  if (length == 0) return false;
  uint32_t parsed = 0;
  for (size_t index = 0; index < length; ++index) {
    if (value[index] < '0' || value[index] > '9') return false;
    const uint32_t digit = value[index] - '0';
    if (parsed > (UINT32_MAX - digit) / 10) return false;
    parsed = parsed * 10 + digit;
  }
  result = parsed;
  return true;
}

bool parsePort(const uint8_t* value, size_t length, uint16_t& result) {
  uint32_t parsed = 0;
  if (!parseU32(value, length, parsed) || parsed == 0 || parsed > UINT16_MAX) return false;
  result = static_cast<uint16_t>(parsed);
  return true;
}

int base64Value(uint8_t value) {
  if (value >= 'A' && value <= 'Z') return value - 'A';
  if (value >= 'a' && value <= 'z') return value - 'a' + 26;
  if (value >= '0' && value <= '9') return value - '0' + 52;
  if (value == '-') return 62;
  if (value == '_') return 63;
  return -1;
}

bool decodeBase64Url(const uint8_t* source, size_t sourceLength, char* destination,
                     size_t capacity, size_t& written) {
  written = 0;
  if (sourceLength % 4 == 1 || destination == nullptr || capacity == 0) return false;
  uint32_t accumulator = 0;
  uint8_t bits = 0;
  for (size_t index = 0; index < sourceLength; ++index) {
    const int value = base64Value(source[index]);
    if (value < 0) return false;
    accumulator = (accumulator << 6) | static_cast<uint32_t>(value);
    bits += 6;
    while (bits >= 8) {
      bits -= 8;
      if (written + 1 >= capacity) return false;
      const uint8_t decoded = static_cast<uint8_t>((accumulator >> bits) & 0xFF);
      // NETCFG v1 carries UTF-8 text and forbids every control character.
      // Reject NUL here as well, otherwise later C-string validation would
      // silently ignore bytes after an embedded terminator.
      if (decoded < 0x20 || decoded == 0x7F) return false;
      destination[written++] = static_cast<char>(decoded);
    }
  }
  if (bits > 0 && (accumulator & ((1U << bits) - 1U)) != 0) return false;
  destination[written] = '\0';
  return true;
}

size_t encodeBase64Url(const char* source, char* destination, size_t capacity) {
  static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  const size_t length = strnlen(source == nullptr ? "" : source, 65);
  size_t output = 0;
  for (size_t index = 0; index < length; index += 3) {
    const size_t remaining = min(static_cast<size_t>(3), length - index);
    const uint32_t group = (static_cast<uint32_t>(static_cast<uint8_t>(source[index])) << 16) |
      (remaining > 1 ? static_cast<uint32_t>(static_cast<uint8_t>(source[index + 1])) << 8 : 0) |
      (remaining > 2 ? static_cast<uint8_t>(source[index + 2]) : 0);
    const size_t count = remaining + 1;
    if (output + count >= capacity) return 0;
    destination[output++] = kAlphabet[(group >> 18) & 0x3F];
    destination[output++] = kAlphabet[(group >> 12) & 0x3F];
    if (remaining > 1) destination[output++] = kAlphabet[(group >> 6) & 0x3F];
    if (remaining > 2) destination[output++] = kAlphabet[group & 0x3F];
  }
  destination[output] = '\0';
  return output;
}
}  // namespace

WifiImageStreamClient::WifiImageStreamClient(const Config& config)
    : config_(config) {}

bool WifiImageStreamClient::begin() {
  if (taskHandle_ != nullptr) {
    return true;
  }
  const NetworkConfigStore::LoadResult load =
      networkConfigStore_.load(config_.defaultNetworkConfig);
  if (load == NetworkConfigStore::LoadResult::Error) {
    return false;
  }
  activeNetworkConfig_ = networkConfigStore_.active();
  activeFromNvs_ = networkConfigStore_.activeFromNvs();
  if (config_.maxJpegBytes == 0 || !psramFound()) {
    return false;
  }

  eventQueue_ = xQueueCreate(config_.eventQueueDepth,
                             sizeof(ImageStreamEvent));
  serialResponseQueue_ = xQueueCreate(4, sizeof(SerialResult));
  if (eventQueue_ == nullptr || serialResponseQueue_ == nullptr) {
    if (eventQueue_ != nullptr) vQueueDelete(eventQueue_);
    eventQueue_ = nullptr;
    if (serialResponseQueue_ != nullptr) vQueueDelete(serialResponseQueue_);
    serialResponseQueue_ = nullptr;
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
      vQueueDelete(serialResponseQueue_);
      serialResponseQueue_ = nullptr;
      return false;
    }
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  // We deliberately own reconnects so failed credentials rotate instead of
  // repeatedly retrying only the last access point.
  WiFi.setAutoReconnect(false);
  if (hasConfiguredNetwork()) startNextWifiConnection();

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
    vQueueDelete(serialResponseQueue_);
    serialResponseQueue_ = nullptr;
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
  // A trial start/failure disconnects the current Wi-Fi/TCP path. Do not use
  // a stale WL_CONNECTED state again in the same service iteration.
  if (serviceTrial(now)) return;
  if (WiFi.status() != WL_CONNECTED) {
    if (client_.connected() || connected_) {
      client_.stop();
      resetParser();
      updateConnected(false);
    }
    if (hasConfiguredNetwork() &&
        now - lastWifiAttemptMs_ >= config_.wifiReconnectMs) {
      startNextWifiConnection();
    }
    return;
  }
  serviceTcp();
}

bool WifiImageStreamClient::serviceTrial(uint32_t now) {
  bool start = false;
  bool expired = false;
  portENTER_CRITICAL(&networkConfigMux_);
  start = trialPending_ && !trialInProgress_ && reached(now, trialStartAtMs_);
  expired = trialInProgress_ && reached(now, trialDeadlineMs_);
  portEXIT_CRITICAL(&networkConfigMux_);
  if (start) {
    startTrial();
    return true;
  }
  if (expired) {
    finishTrialFailure();
    return true;
  }
  return false;
}

bool WifiImageStreamClient::hasConfiguredNetwork() const {
  return workingNetworkConfig().valid();
}

bool WifiImageStreamClient::startNextWifiConnection() {
  const NetworkConfigData networkConfig = workingNetworkConfig();
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
    portENTER_CRITICAL(&networkConfigMux_);
    selectedWifiNetworkIndex_ = static_cast<uint8_t>(index);
    portEXIT_CRITICAL(&networkConfigMux_);
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
    const NetworkConfigData networkConfig = workingNetworkConfig();
    if (!client_.connect(networkConfig.host, networkConfig.port, 500)) {
      client_.stop();
      return;
    }
    client_.setNoDelay(true);
    resetParser();
    const uint8_t* id = reinterpret_cast<const uint8_t*>(config_.deviceId);
    const size_t idLength = config_.deviceId == nullptr
                                ? 0
                                : strlen(config_.deviceId);
    if (!sendMessage(MessageType::Hello, 0, id, idLength)) {
      client_.stop();
      updateConnected(false);
      return;
    }
    updateConnected(true);
    bool trial = false;
    portENTER_CRITICAL(&networkConfigMux_);
    trial = trialInProgress_;
    portEXIT_CRITICAL(&networkConfigMux_);
    if (trial) {
      finishTrialSuccess();
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
       header_[4] != kProtocolVersion || readU16(header_ + 6) != 0) {
    return false;
  }

  currentType_ = static_cast<MessageType>(header_[5]);
  currentSequence_ = readU32(header_ + 8);
  currentPayloadLength_ = readU32(header_ + 12);
  currentPayloadReceived_ = 0;
  writingSlot_ = -1;
  discardPayload_ = false;

  switch (currentType_) {
    case MessageType::Ping:
    case MessageType::Pong:
    case MessageType::StreamEnd:
      return currentPayloadLength_ == 0;
    case MessageType::StreamStart:
      // !IIHH: duration, hold, target FPS and reserved (which must be zero).
      return currentPayloadLength_ == 12;
    case MessageType::JpegFrame:
      if (currentPayloadLength_ < 4 ||
          currentPayloadLength_ > config_.maxJpegBytes) {
        return false;
      }
      writingSlot_ = acquireFrameSlot();
      discardPayload_ = writingSlot_ < 0;
      break;
    case MessageType::Hello:
      // HELLO is sent by the ESP32 when it connects; it is not a server command.
      return false;
    default:
      return false;
  }
  return true;
}

void WifiImageStreamClient::finishMessage() {
  switch (currentType_) {
    case MessageType::StreamStart: {
      if (readU16(controlPayload_ + 10) != 0) {
        ImageStreamEvent error;
        error.type = ImageStreamEventType::ProtocolError;
        error.sequence = currentSequence_;
        queueEvent(error);
        client_.stop();
        break;
      }
      ImageStreamEvent event;
      event.type = ImageStreamEventType::StreamStarted;
      event.sequence = currentSequence_;
      event.streamDurationMs = readU32(controlPayload_);
      event.holdDurationMs = readU32(controlPayload_ + 4);
      event.targetFps = readU16(controlPayload_ + 8);
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
    case MessageType::Ping:
      sendMessage(MessageType::Pong, currentSequence_, nullptr, 0);
      break;
    case MessageType::Hello:
    case MessageType::Pong:
      break;
  }
}

NetworkConfigData WifiImageStreamClient::workingNetworkConfig() const {
  portENTER_CRITICAL(&networkConfigMux_);
  const NetworkConfigData result = trialInProgress_ ? candidateNetworkConfig_
                                                     : activeNetworkConfig_;
  portEXIT_CRITICAL(&networkConfigMux_);
  return result;
}

void WifiImageStreamClient::queueSerialResult(uint32_t sequence,
                                              ConfigResult result,
                                              uint8_t detail) {
  if (serialResponseQueue_ == nullptr) return;
  const SerialResult item = {sequence, static_cast<uint8_t>(result), detail};
  xQueueSend(serialResponseQueue_, &item, 0);
}

void WifiImageStreamClient::startTrial() {
  portENTER_CRITICAL(&networkConfigMux_);
  trialPending_ = false;
  trialStartAtMs_ = 0;
  if (!candidatePresent_ || !candidateNetworkConfig_.valid()) {
    portEXIT_CRITICAL(&networkConfigMux_);
    queueSerialResult(trialSequence_, ConfigResult::NoCandidate);
    return;
  }
  trialInProgress_ = true;
  trialDeadlineMs_ = millis() + kNetworkTrialTimeoutMs;
  nextWifiNetworkIndex_ = 0;
  selectedWifiNetworkIndex_ = 0xFF;
  portEXIT_CRITICAL(&networkConfigMux_);
  client_.stop();
  resetParser();
  updateConnected(false);
  WiFi.disconnect(false, false);
  lastWifiAttemptMs_ = millis() - config_.wifiReconnectMs;
  // Start the first candidate immediately. Besides reducing provisioning
  // latency, issuing WiFi.begin() here prevents a transient old
  // WL_CONNECTED status from validating the candidate TCP endpoint.
  startNextWifiConnection();
}

void WifiImageStreamClient::finishTrialSuccess() {
  NetworkConfigData candidate;
  portENTER_CRITICAL(&networkConfigMux_);
  candidate = candidateNetworkConfig_;
  portEXIT_CRITICAL(&networkConfigMux_);
  if (!networkConfigStore_.saveActive(candidate)) {
    finishTrialFailure(ConfigResult::StoreError);
    return;
  }
  portENTER_CRITICAL(&networkConfigMux_);
  activeNetworkConfig_ = networkConfigStore_.active();
  activeFromNvs_ = true;
  trialInProgress_ = false;
  trialDeadlineMs_ = 0;
  candidatePresent_ = false;
  const uint32_t sequence = trialSequence_;
  portEXIT_CRITICAL(&networkConfigMux_);
  queueSerialResult(sequence, ConfigResult::TrialConnectedSaved);
}

void WifiImageStreamClient::finishTrialFailure(ConfigResult result) {
  portENTER_CRITICAL(&networkConfigMux_);
  if (!trialInProgress_ && !trialPending_) {
    portEXIT_CRITICAL(&networkConfigMux_);
    return;
  }
  trialPending_ = false;
  trialInProgress_ = false;
  trialStartAtMs_ = 0;
  trialDeadlineMs_ = 0;
  nextWifiNetworkIndex_ = 0;
  selectedWifiNetworkIndex_ = 0xFF;
  const uint32_t sequence = trialSequence_;
  portEXIT_CRITICAL(&networkConfigMux_);
  client_.stop();
  resetParser();
  updateConnected(false);
  WiFi.disconnect(false, false);
  lastWifiAttemptMs_ = millis() - config_.wifiReconnectMs;
  queueSerialResult(sequence, result);
}

bool WifiImageStreamClient::handleSerialCommand(const uint8_t* data,
                                                size_t length,
                                                char* response,
                                                size_t capacity,
                                                bool& applyAccepted) {
  applyAccepted = false;
  if (data == nullptr || response == nullptr || capacity == 0) return false;
  const char* operations[] = {"SET", "APPLY", "QUERY"};
  const char* prefixes[] = {"netcfg:set:", "netcfg:apply:", "netcfg:query:"};
  size_t operation = 3;
  for (size_t index = 0; index < 3; ++index) {
    const size_t prefixLength = strlen(prefixes[index]);
    if (length >= prefixLength && asciiEqualsIgnoreCase(data, prefixLength, prefixes[index])) {
      operation = index;
      data += prefixLength;
      length -= prefixLength;
      break;
    }
  }
  if (operation == 3) return false;

  const uint8_t* fields[10] = {0};
  size_t fieldLengths[10] = {0};
  size_t count = 0;
  size_t start = 0;
  for (size_t index = 0; index <= length; ++index) {
    if (index != length && data[index] != '|') continue;
    if (count >= 10) break;
    fields[count] = data + start;
    fieldLengths[count++] = index - start;
    start = index + 1;
  }
  uint32_t sequence = 0;
  const bool sequenceOk = count > 0 && parseU32(fields[0], fieldLengths[0], sequence);
  auto resultLine = [&](ConfigResult result, uint8_t detail) {
    snprintf(response, capacity, "NETCFG:RESULT:%lu|%s|%u|%u",
             static_cast<unsigned long>(sequence), operations[operation],
             static_cast<unsigned>(result), static_cast<unsigned>(detail));
  };
  if (!sequenceOk || (operation == 0 && count != 10) ||
      ((operation == 1 || operation == 2) && count != 2) ||
      (count > 0 && start <= length)) {
    resultLine(ConfigResult::ValidationError,
               static_cast<uint8_t>(ConfigValidationDetail::WifiCount));
    return true;
  }
  if (fieldLengths[1] != 1 || fields[1][0] != '1') {
    resultLine(ConfigResult::ValidationError,
               static_cast<uint8_t>(ConfigValidationDetail::Version));
    return true;
  }

  if (operation == 2) {
    NetworkConfigData active;
    bool activeFromNvs = false, candidate = false, busy = false;
    uint8_t selected = 0xFF;
    portENTER_CRITICAL(&networkConfigMux_);
    active = activeNetworkConfig_;
    activeFromNvs = activeFromNvs_;
    candidate = candidatePresent_;
    busy = trialPending_ || trialInProgress_;
    selected = selectedWifiNetworkIndex_;
    portEXIT_CRITICAL(&networkConfigMux_);
    char encoded[3][48] = {{0}}, host[88] = {0};
    for (size_t index = 0; index < 3; ++index)
      if (encodeBase64Url(active.wifi[index].ssid, encoded[index], sizeof(encoded[index])) == 0 && active.wifi[index].ssid[0] != '\0') {
        resultLine(ConfigResult::StoreError, static_cast<uint8_t>(ConfigValidationDetail::Length)); return true;
      }
    if (encodeBase64Url(active.host, host, sizeof(host)) == 0 && active.host[0] != '\0') {
      resultLine(ConfigResult::StoreError, static_cast<uint8_t>(ConfigValidationDetail::Length)); return true;
    }
    const uint8_t flags = (activeFromNvs ? 1 : 0) | (candidate ? 2 : 0) | (busy ? 4 : 0);
    snprintf(response, capacity, "NETCFG:STATUS:%lu|1|%u|%u|%s|%s|%s|%s|%u",
             static_cast<unsigned long>(sequence), static_cast<unsigned>(flags),
             static_cast<unsigned>(selected), encoded[0], encoded[1], encoded[2], host,
             static_cast<unsigned>(active.port));
    return true;
  }

  if (operation == 1) {
    portENTER_CRITICAL(&networkConfigMux_);
    const bool busy = trialPending_ || trialInProgress_;
    const bool available = candidatePresent_ && candidateNetworkConfig_.valid();
    if (!busy && available) trialSequence_ = sequence;
    portEXIT_CRITICAL(&networkConfigMux_);
    if (busy) resultLine(ConfigResult::ValidationError, static_cast<uint8_t>(ConfigValidationDetail::Length));
    else if (!available) resultLine(ConfigResult::NoCandidate, 0);
    else { resultLine(ConfigResult::ApplyAccepted, 0); applyAccepted = true; }
    return true;
  }

  NetworkConfigData parsed;
  parsed.version = NetworkConfigData::kVersion;
  for (size_t index = 0; index < 3; ++index) {
    size_t decoded = 0;
    if (!decodeBase64Url(fields[2 + index * 2], fieldLengths[2 + index * 2],
                         parsed.wifi[index].ssid, sizeof(parsed.wifi[index].ssid), decoded) ||
        decoded > NetworkConfigData::kMaxSsidBytes) {
      resultLine(ConfigResult::ValidationError, static_cast<uint8_t>(ConfigValidationDetail::SsidLength)); return true;
    }
    if (!decodeBase64Url(fields[3 + index * 2], fieldLengths[3 + index * 2],
                         parsed.wifi[index].password, sizeof(parsed.wifi[index].password), decoded) ||
        decoded > NetworkConfigData::kMaxPasswordBytes) {
      resultLine(ConfigResult::ValidationError, static_cast<uint8_t>(ConfigValidationDetail::PasswordLength)); return true;
    }
  }
  size_t decoded = 0;
  if (!decodeBase64Url(fields[8], fieldLengths[8], parsed.host, sizeof(parsed.host), decoded) || decoded == 0 || decoded > NetworkConfigData::kMaxHostBytes) {
    resultLine(ConfigResult::ValidationError, static_cast<uint8_t>(ConfigValidationDetail::HostLength)); return true;
  }
  if (!parsePort(fields[9], fieldLengths[9], parsed.port)) {
    resultLine(ConfigResult::ValidationError, static_cast<uint8_t>(ConfigValidationDetail::Port)); return true;
  }
  if (!parsed.valid()) {
    resultLine(ConfigResult::ValidationError, static_cast<uint8_t>(ConfigValidationDetail::NoWifi)); return true;
  }
  portENTER_CRITICAL(&networkConfigMux_);
  const bool busy = trialPending_ || trialInProgress_;
  if (!busy) { candidateNetworkConfig_ = parsed; candidatePresent_ = true; }
  portEXIT_CRITICAL(&networkConfigMux_);
  if (busy) resultLine(ConfigResult::ValidationError, static_cast<uint8_t>(ConfigValidationDetail::Length));
  else resultLine(ConfigResult::Staged, 0);
  return true;
}

void WifiImageStreamClient::acknowledgeApplyOutput() {
  portENTER_CRITICAL(&networkConfigMux_);
  if (!trialPending_ && !trialInProgress_ && candidatePresent_ && candidateNetworkConfig_.valid()) {
    trialStartAtMs_ = millis() + kNetworkTrialDelayMs;
    trialPending_ = true;
  }
  portEXIT_CRITICAL(&networkConfigMux_);
}

bool WifiImageStreamClient::pollSerialResponse(char* response, size_t capacity) {
  SerialResult result;
  if (response == nullptr || capacity == 0 || serialResponseQueue_ == nullptr ||
      xQueueReceive(serialResponseQueue_, &result, 0) != pdTRUE) return false;
  snprintf(response, capacity, "NETCFG:RESULT:%lu|APPLY|%u|%u",
           static_cast<unsigned long>(result.sequence), static_cast<unsigned>(result.result),
           static_cast<unsigned>(result.detail));
  return true;
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
