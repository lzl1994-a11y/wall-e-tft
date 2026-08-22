#include "adapters/display/ScreenFontFlash.h"
#include <string.h>

namespace WallE {

namespace {
/// 中文：SPI Flash 标准读取数据命令。
/// English: Standard SPI flash read-data command.
constexpr uint8_t kReadData = 0x03;

/// 中文：退出低功耗/唤醒命令。
/// English: Release-from-power-down / wake-up command.
constexpr uint8_t kReleasePowerDown = 0xAB;

/// 中文：一个 8x16 单字节字模占用 16 字节。
/// English: One 8x16 single-byte glyph uses 16 bytes.
constexpr size_t kAscii8x16Size = 16;

/// 中文：一个 16x16 双字节字模占用 32 字节。
/// English: One 16x16 double-byte glyph uses 32 bytes.
constexpr size_t kDoubleByte16x16Size = 32;

/// 中文：字库镜像签名，用于确认 Flash 中烧录的是预期资源。
/// English: Font image signature used to verify the flash contains the expected resource.
const uint8_t kSignature[] = {'J', 'Y', 'C', '-', '4', 'M', 'b', 'B', 'y', 't', 'e', '-',
                              'F', 'O', 'N', 'T', '-', 'F', 'L', 'A', 'S', 'H'};
}  // namespace

ScreenFontFlash::ScreenFontFlash(const Config& config) : config_(config) {}

ScreenFontFlash::~ScreenFontFlash() {
  if (device_ != nullptr) {
    spi_bus_remove_device(device_);
    device_ = nullptr;
  }
}

bool ScreenFontFlash::begin() {
  initialized_ = false;
  available_ = false;
  if (config_.csPin < 0 || config_.sckPin < 0 || config_.misoPin < 0 ||
      config_.mosiPin < 0) {
    return false;
  }

  pinMode(config_.csPin, OUTPUT);
  digitalWrite(config_.csPin, HIGH);

  spi_device_interface_config_t deviceConfig = {};
  deviceConfig.mode = 0;
  deviceConfig.clock_speed_hz = static_cast<int>(config_.spiHz);
  deviceConfig.spics_io_num = -1;
  deviceConfig.queue_size = 1;
  deviceConfig.flags = SPI_DEVICE_NO_DUMMY;
  if (spi_bus_add_device(static_cast<spi_host_device_t>(config_.spiHost),
                         &deviceConfig, &device_) != ESP_OK) {
    device_ = nullptr;
    return false;
  }

  if (!select()) {
    return false;
  }
  uint8_t wakeBytes[] = {kReleasePowerDown, 0x00, 0x00, 0x00, 0x00};
  spi_transaction_t wake = {};
  wake.length = sizeof(wakeBytes) * 8;
  wake.tx_buffer = wakeBytes;
  const esp_err_t wakeResult = spi_device_polling_transmit(device_, &wake);
  deselect();
  if (wakeResult != ESP_OK) {
    return false;
  }
  delay(2);

  initialized_ = true;
  available_ = checkSignature();
  return true;
}

bool ScreenFontFlash::checkSignature() {
  uint8_t data[sizeof(kSignature)] = {0};
  if (!read(0, data, sizeof(data))) {
    return false;
  }
  return memcmp(data, kSignature, sizeof(kSignature)) == 0;
}

bool ScreenFontFlash::read(uint32_t address, uint8_t* buffer, size_t length) {
  if (!initialized_ || buffer == nullptr || length == 0) {
    return false;
  }

  if (!select()) {
    return false;
  }

  spi_transaction_t command = {};
  command.flags = SPI_TRANS_USE_TXDATA;
  command.length = 32;
  command.tx_data[0] = kReadData;
  command.tx_data[1] = static_cast<uint8_t>(address >> 16);
  command.tx_data[2] = static_cast<uint8_t>(address >> 8);
  command.tx_data[3] = static_cast<uint8_t>(address);
  esp_err_t result = spi_device_polling_transmit(device_, &command);

  if (result == ESP_OK) {
    spi_transaction_t payload = {};
    payload.length = length * 8;
    payload.rxlength = length * 8;
    payload.rx_buffer = buffer;
    result = spi_device_polling_transmit(device_, &payload);
  }
  deselect();
  return result == ESP_OK;
}

bool ScreenFontFlash::readAscii8x16(uint8_t code, uint8_t* buffer, size_t length) {
  if (buffer == nullptr || length < kAscii8x16Size || !initialized_) {
    return false;
  }

  return read(config_.ascii8x16Base + static_cast<uint32_t>(code) * kAscii8x16Size, buffer,
              kAscii8x16Size);
}

bool ScreenFontFlash::readDoubleByte16x16(uint8_t high, uint8_t low, uint8_t* buffer,
                                          size_t length) {
  if (buffer == nullptr || length < kDoubleByte16x16Size || !initialized_) {
    return false;
  }
  if (high < config_.doubleByteFirst || low < config_.doubleByteFirst ||
      config_.doubleByteColumns == 0) {
    return false;
  }

  const uint32_t row = static_cast<uint32_t>(high - config_.doubleByteFirst);
  const uint32_t column = static_cast<uint32_t>(low - config_.doubleByteFirst);
  const uint32_t index = row * config_.doubleByteColumns + column;
  return read(config_.doubleByte16x16Base + index * kDoubleByte16x16Size, buffer,
              kDoubleByte16x16Size);
}

bool ScreenFontFlash::select() {
  if (device_ == nullptr ||
      spi_device_acquire_bus(device_, portMAX_DELAY) != ESP_OK) {
    return false;
  }
  digitalWrite(config_.csPin, LOW);
  return true;
}

void ScreenFontFlash::deselect() {
  digitalWrite(config_.csPin, HIGH);
  if (device_ != nullptr) {
    spi_device_release_bus(device_);
  }
}

}  // namespace WallE
