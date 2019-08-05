//
//  time.h
//
//  Created by Pujun Lun on 5/11/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_TIME_H
#define JESSIE_STEAMER_COMMON_TIME_H

#include <chrono>

namespace jessie_steamer {
namespace common {

// Timer is used for tracking the frame rate.
class Timer {
 public:
  Timer() : frame_count_{0}, frame_rate_{0} {
    launch_time_ = last_update_time_ = last_frame_time_ = Now();
  }

  // Informs the timer that a frame has been rendered. The frame rate is updated
  // per second.
  void Tick() {
    ++frame_count_;
    last_frame_time_ = Now();
    if (TimeInterval(last_update_time_, last_frame_time_) >= 1.0f) {
      last_update_time_ = last_frame_time_;
      frame_rate_ = frame_count_;
      frame_count_ = 0;
    }
  }

  // Returns the time elapsed since the timer was launched in second.
  float GetElapsedTimeSinceLaunch() const {
    return TimeInterval(launch_time_, Now());
  }

  // Returns the time elapsed since the last frame was rendered in second.
  float GetElapsedTimeSinceLastFrame() const {
    return TimeInterval(last_frame_time_, Now());
  }

  // Accessors.
  int frame_rate() const { return frame_rate_; }

 private:
  using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

  // Returns the current time point.
  static TimePoint Now() { return std::chrono::high_resolution_clock::now(); }

  // Returns the length of interval between two time points in second.
  static float TimeInterval(const TimePoint& t1, const TimePoint& t2) {
    return std::chrono::duration<
        float, std::chrono::seconds::period>(t2 - t1).count();
  }

  // The time point when the timer was launched.
  TimePoint launch_time_;

  // The time point when the frame rate was last updated.
  TimePoint last_update_time_;

  // The time point when the last frame was rendered.
  TimePoint last_frame_time_;

  // The number of frames that have been rendered since 'last_update_time_'.
  int frame_count_;

  // The number of frames rendered per second.
  int frame_rate_;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_TIME_H */
