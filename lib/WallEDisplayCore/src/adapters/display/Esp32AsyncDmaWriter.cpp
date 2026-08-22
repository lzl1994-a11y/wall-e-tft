#include "adapters/display/Esp32AsyncDmaWriter.h"

#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <string.h>

namespace WallE {

namespace {
constexpr uint8_t kSetColumnAddress = 0x2A;
constexpr uint8_t kSetRowAddress = 0x2B;
constexpr uint8_t kWriteMemory = 0x2C;
}  // namespace

Esp32AsyncDmaWriter::Esp32AsyncDmaWriter(int8_t dcPin, int8_t csPin,
                                         uint8_t spiHost, uint32_t speedHz,
                                         int16_t xOffset, int16_t yOffset)
    : dcPin_(dcPin),
      csPin_(csPin),
      spiHost_(spiHost),
      speedHz_(speedHz),
      xOffset_(xOffset),
      yOffset_(yOffset) {}

Esp32AsyncDmaWriter::~Esp32AsyncDmaWriter() {
  end();
}

bool Esp32AsyncDmaWriter::begin() {
  if (ready_) {
    return true;
  }
  if (dcPin_ < 0 || csPin_ < 0 || speedHz_ == 0) {
    return false;
  }

  pinMode(dcPin_, OUTPUT);
  pinMode(csPin_, OUTPUT);
  digitalWrite(dcPin_, HIGH);
  digitalWrite(csPin_, HIGH);

  spi_device_interface_config_t deviceConfig = {};
  deviceConfig.mode = 0;
  deviceConfig.clock_speed_hz = static_cast<int>(speedHz_);
  deviceConfig.spics_io_num = -1;
  deviceConfig.queue_size = kQueueSize;
  deviceConfig.flags = SPI_DEVICE_NO_DUMMY;
  deviceConfig.pre_cb = onPreTransfer;

  if (spi_bus_add_device(static_cast<spi_host_device_t>(spiHost_),
                         &deviceConfig, &device_) != ESP_OK) {
    device_ = nullptr;
    return false;
  }

  for (size_t index = 0; index < kBufferCount; ++index) {
    BufferSlot& slot = buffers_[index];
    slot.pixels = static_cast<uint16_t*>(heap_caps_malloc(
        kPixelsPerBuffer * sizeof(uint16_t), MALLOC_CAP_DMA));
    if (slot.pixels == nullptr) {
      end();
      return false;
    }
    for (size_t tx = 0; tx < kTransactionsPerBuffer; ++tx) {
      slot.contexts[tx].owner = this;
      slot.contexts[tx].bufferIndex = static_cast<uint8_t>(index);
    }
  }

  pendingTransactions_ = 0;
  ready_ = true;
  return true;
}

void Esp32AsyncDmaWriter::end() {
  if (frameActive_) {
    endFrame();
  } else if (device_ != nullptr) {
    waitForAll();
  }

  if (device_ != nullptr) {
    spi_bus_remove_device(device_);
    device_ = nullptr;
  }
  for (BufferSlot& slot : buffers_) {
    if (slot.pixels != nullptr) {
      heap_caps_free(slot.pixels);
      slot.pixels = nullptr;
    }
    slot.pendingTransactions = 0;
  }
  pendingTransactions_ = 0;
  ready_ = false;
}

bool Esp32AsyncDmaWriter::beginFrame() {
  if (!ready_ || frameActive_ || device_ == nullptr) {
    return false;
  }

  waitForAll();
  if (spi_device_acquire_bus(device_, portMAX_DELAY) != ESP_OK) {
    return false;
  }
  digitalWrite(dcPin_, HIGH);
  digitalWrite(csPin_, LOW);
  frameActive_ = true;
  return true;
}

uint16_t* Esp32AsyncDmaWriter::acquireBuffer(size_t index) {
  if (!frameActive_ || index >= kBufferCount || !waitForBuffer(index)) {
    return nullptr;
  }
  return buffers_[index].pixels;
}

bool Esp32AsyncDmaWriter::queueRect(size_t bufferIndex, int16_t x,
                                    int16_t y, int16_t width,
                                    int16_t height, size_t pixelCount) {
  if (!frameActive_ || bufferIndex >= kBufferCount || x < 0 || y < 0 ||
      width <= 0 || height <= 0 || pixelCount == 0 ||
      pixelCount > kPixelsPerBuffer ||
      pixelCount != static_cast<size_t>(width) * height) {
    return false;
  }

  BufferSlot& slot = buffers_[bufferIndex];
  if (!waitForBuffer(bufferIndex)) {
    return false;
  }

  const uint16_t xStart = static_cast<uint16_t>(x + xOffset_);
  const uint16_t yStart = static_cast<uint16_t>(y + yOffset_);
  const uint16_t xEnd = static_cast<uint16_t>(xStart + width - 1);
  const uint16_t yEnd = static_cast<uint16_t>(yStart + height - 1);
  slot.columnData[0] = static_cast<uint8_t>(xStart >> 8);
  slot.columnData[1] = static_cast<uint8_t>(xStart);
  slot.columnData[2] = static_cast<uint8_t>(xEnd >> 8);
  slot.columnData[3] = static_cast<uint8_t>(xEnd);
  slot.rowData[0] = static_cast<uint8_t>(yStart >> 8);
  slot.rowData[1] = static_cast<uint8_t>(yStart);
  slot.rowData[2] = static_cast<uint8_t>(yEnd >> 8);
  slot.rowData[3] = static_cast<uint8_t>(yEnd);

  for (spi_transaction_t& transaction : slot.transactions) {
    memset(&transaction, 0, sizeof(transaction));
  }

  slot.contexts[0].dataMode = false;
  slot.transactions[0].flags = SPI_TRANS_USE_TXDATA;
  slot.transactions[0].length = 8;
  slot.transactions[0].tx_data[0] = kSetColumnAddress;

  slot.contexts[1].dataMode = true;
  slot.transactions[1].tx_buffer = slot.columnData;
  slot.transactions[1].length = sizeof(slot.columnData) * 8;

  slot.contexts[2].dataMode = false;
  slot.transactions[2].flags = SPI_TRANS_USE_TXDATA;
  slot.transactions[2].length = 8;
  slot.transactions[2].tx_data[0] = kSetRowAddress;

  slot.contexts[3].dataMode = true;
  slot.transactions[3].tx_buffer = slot.rowData;
  slot.transactions[3].length = sizeof(slot.rowData) * 8;

  slot.contexts[4].dataMode = false;
  slot.transactions[4].flags = SPI_TRANS_USE_TXDATA;
  slot.transactions[4].length = 8;
  slot.transactions[4].tx_data[0] = kWriteMemory;

  slot.contexts[5].dataMode = true;
  slot.transactions[5].tx_buffer = slot.pixels;
  slot.transactions[5].length = pixelCount * 16;

  for (size_t tx = 0; tx < kTransactionsPerBuffer; ++tx) {
    slot.transactions[tx].user = &slot.contexts[tx];
    if (!queueTransaction(slot, tx)) {
      return false;
    }
  }
  return true;
}

void Esp32AsyncDmaWriter::endFrame() {
  if (!frameActive_) {
    return;
  }
  waitForAll();
  digitalWrite(csPin_, HIGH);
  spi_device_release_bus(device_);
  frameActive_ = false;
}

void IRAM_ATTR Esp32AsyncDmaWriter::onPreTransfer(
    spi_transaction_t* transaction) {
  if (transaction == nullptr || transaction->user == nullptr) {
    return;
  }
  const TransferContext* context =
      static_cast<const TransferContext*>(transaction->user);
  if (context->owner == nullptr) {
    return;
  }
  gpio_set_level(static_cast<gpio_num_t>(context->owner->dcPin_),
                 context->dataMode ? 1 : 0);
}

bool Esp32AsyncDmaWriter::queueTransaction(BufferSlot& slot,
                                            size_t transactionIndex) {
  if (spi_device_queue_trans(device_, &slot.transactions[transactionIndex],
                             portMAX_DELAY) != ESP_OK) {
    return false;
  }
  ++slot.pendingTransactions;
  ++pendingTransactions_;
  return true;
}

bool Esp32AsyncDmaWriter::waitForBuffer(size_t index) {
  while (buffers_[index].pendingTransactions > 0) {
    if (!reapOne(portMAX_DELAY)) {
      return false;
    }
  }
  return true;
}

bool Esp32AsyncDmaWriter::reapOne(TickType_t waitTicks) {
  spi_transaction_t* completed = nullptr;
  if (spi_device_get_trans_result(device_, &completed, waitTicks) != ESP_OK ||
      completed == nullptr || completed->user == nullptr) {
    return false;
  }

  const TransferContext* context =
      static_cast<const TransferContext*>(completed->user);
  if (context->owner == this && context->bufferIndex < kBufferCount) {
    BufferSlot& slot = buffers_[context->bufferIndex];
    if (slot.pendingTransactions > 0) {
      --slot.pendingTransactions;
    }
  }
  if (pendingTransactions_ > 0) {
    --pendingTransactions_;
  }
  return true;
}

void Esp32AsyncDmaWriter::waitForAll() {
  while (pendingTransactions_ > 0) {
    if (!reapOne(portMAX_DELAY)) {
      break;
    }
  }
}

}  // namespace WallE
