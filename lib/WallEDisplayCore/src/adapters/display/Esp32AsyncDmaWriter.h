#pragma once

#include "ports/IAsyncBitmapWriter.h"

#include <Arduino.h>
#include <driver/spi_master.h>
#include <stddef.h>
#include <stdint.h>

namespace WallE {

class Esp32AsyncDmaWriter final : public IAsyncBitmapWriter {
 public:
  static constexpr size_t kBufferCount = 2;
  static constexpr size_t kChunkRows = 16;
  static constexpr size_t kMaxWidth = 240;
  static constexpr size_t kPixelsPerBuffer = kMaxWidth * kChunkRows;

  Esp32AsyncDmaWriter(int8_t dcPin, int8_t csPin, uint8_t spiHost,
                      uint32_t speedHz);
  ~Esp32AsyncDmaWriter() override;

  bool begin();
  void end();
  bool ready() const { return ready_; }

  bool beginFrame() override;
  uint16_t* acquireBuffer(size_t index) override;
  size_t bufferCount() const override { return kBufferCount; }
  size_t bufferPixelCapacity() const override { return kPixelsPerBuffer; }
  bool queueRect(size_t bufferIndex, int16_t x, int16_t y,
                 int16_t width, int16_t height,
                 size_t pixelCount) override;
  void endFrame() override;

 private:
  static constexpr size_t kTransactionsPerBuffer = 6;
  static constexpr int kQueueSize =
      kBufferCount * kTransactionsPerBuffer;

  struct TransferContext {
    Esp32AsyncDmaWriter* owner = nullptr;
    uint8_t bufferIndex = 0;
    bool dataMode = true;
  };

  struct BufferSlot {
    uint16_t* pixels = nullptr;
    spi_transaction_t transactions[kTransactionsPerBuffer] = {};
    TransferContext contexts[kTransactionsPerBuffer] = {};
    alignas(4) uint8_t columnData[4] = {};
    alignas(4) uint8_t rowData[4] = {};
    size_t pendingTransactions = 0;
  };

  static void IRAM_ATTR onPreTransfer(spi_transaction_t* transaction);
  bool queueTransaction(BufferSlot& slot, size_t transactionIndex);
  bool waitForBuffer(size_t index);
  bool reapOne(TickType_t waitTicks);
  void waitForAll();

  int8_t dcPin_;
  int8_t csPin_;
  uint8_t spiHost_;
  uint32_t speedHz_;
  spi_device_handle_t device_ = nullptr;
  BufferSlot buffers_[kBufferCount];
  size_t pendingTransactions_ = 0;
  bool ready_ = false;
  bool frameActive_ = false;
};

}  // namespace WallE
