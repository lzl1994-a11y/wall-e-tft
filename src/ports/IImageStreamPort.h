#pragma once

#include "domain/ImageStreamEvent.h"

namespace WallE {

/** Hot-standby image stream input. Network work runs outside the UI task. */
class IImageStreamPort {
 public:
  virtual ~IImageStreamPort() = default;

  virtual bool begin() = 0;
  virtual bool poll(ImageStreamEvent& event) = 0;
  virtual void releaseFrame(uint8_t frameToken) = 0;
  virtual bool connected() const = 0;
};

}  // namespace WallE
