#pragma once

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
   * 中文：播放 zoom 眼睛动作，目前对应内置 GIF 动画。
   * English: Plays the zoom eye action, currently backed by the embedded GIF animation.
   *
   * @param 无 / None.
   * @return 无 / None.
   */
  virtual void playZoom() = 0;
};

}  // namespace WallE
