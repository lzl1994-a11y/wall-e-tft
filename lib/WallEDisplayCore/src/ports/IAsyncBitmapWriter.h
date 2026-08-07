#pragma once

#include <stddef.h>
#include <stdint.h>

namespace WallE {

class IAsyncBitmapWriter {
 public:
  virtual ~IAsyncBitmapWriter() = default;

  virtual bool beginFrame() = 0;
  virtual uint16_t* acquireBuffer(size_t index) = 0;
  virtual size_t bufferCount() const = 0;
  virtual size_t bufferPixelCapacity() const = 0;
  virtual bool queueRect(size_t bufferIndex, int16_t x, int16_t y,
                         int16_t width, int16_t height,
                         size_t pixelCount) = 0;
  virtual void endFrame() = 0;
};

}  // namespace WallE
