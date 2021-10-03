//
//  timer.h
//
//  Created by Pujun Lun on 5/11/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_COMMON_TIMER_H
#define LIGHTER_COMMON_TIMER_H

#include <chrono>

namespace lighter::common {

// This is used to get the elapsed time since the timer is launched.
class BasicTimer {
 public:
  explicit BasicTimer() : launch_time_{Now()} {}

  // This class is neither copyable nor movable.
  BasicTimer(const BasicTimer&) = delete;
  BasicTimer& operator=(const BasicTimer&) = delete;

  // Returns the time elapsed since the timer was launched in second.
  float GetElapsedTimeSinceLaunch() const {
    return TimeInterval(launch_time_, Now());
  }

 protected:
  using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

  // Returns the current time point.
  static TimePoint Now() { return std::chrono::high_resolution_clock::now(); }

  // Returns the length of interval between two time points in second.
  static float TimeInterval(const TimePoint& t1, const TimePoint& t2) {
    return std::chrono::duration<
        float, std::chrono::seconds::period>(t2 - t1).count();
  }

  // Time point when the timer was launched.
  const TimePoint launch_time_;
};

// This is used for tracking the frame rate.
class FrameTimer : public BasicTimer {
 public:
  explicit FrameTimer() : frame_count_{0}, frame_rate_{0} {
    last_update_time_ = last_frame_time_ = launch_time_;
  }

  // This class is neither copyable nor movable.
  FrameTimer(const FrameTimer&) = delete;
  FrameTimer& operator=(const FrameTimer&) = delete;

  // Informs the timer that we have finished rendering a new frame.
  // The frame rate is updated per second.
  void Tick() {
    ++frame_count_;
    last_frame_time_ = Now();
    if (TimeInterval(last_update_time_, last_frame_time_) >= 1.0f) {
      last_update_time_ = last_frame_time_;
      frame_rate_ = frame_count_;
      frame_count_ = 0;
    }
  }

  // Returns the time elapsed since the last frame was rendered in second.
  float GetElapsedTimeSinceLastFrame() const {
    return TimeInterval(last_frame_time_, Now());
  }

  // Accessors.
  int frame_rate() const { return frame_rate_; }

 private:
  // Time point when the frame rate was last updated.
  TimePoint last_update_time_;

  // Time point when the last frame was rendered.
  TimePoint last_frame_time_;

  // Number of frames that have been rendered since 'last_update_time_'.
  int frame_count_;

  // Number of frames rendered per second.
  int frame_rate_;
};

}  // namespace lighter::common

#endif  // LIGHTER_COMMON_TIMER_H
