#pragma once

#include "domain/EyeRenderer.h"

namespace WallE {

/**
 * 中文：眼睛显示屏抽象接口；用于把眼睛动作与应用控制器解耦。
 * English: Eye display abstraction; decouples eye actions from the application controller.
 */
class IEyeDisplayPort {
 public:
  /**
   * 中文：虚析构函数，用于接口多态。
   * English: Virtual destructor for polymorphic use.
   */
  virtual ~IEyeDisplayPort() = default;

  /**
   * 中文：触发光核实时缩放动作。
   * English: Triggers a live light-core zoom action.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  virtual void playZoom() = 0;

  virtual void blink() = 0;
  virtual bool configure(const uint8_t* data, size_t length) = 0;
  virtual const EyeSettings& settings() const = 0;
  virtual bool renderOk() const = 0;

  /**
   * 中文：推进眼睛动画播放状态；应在主循环中周期性调用。
   * English: Advances eye animation playback state; call periodically from the main loop.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  virtual void update() = 0;
};

}  // namespace WallE
