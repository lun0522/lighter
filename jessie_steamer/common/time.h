//
//  time.h
//
//  Created by Pujun Lun on 5/11/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_TIME_H
#define JESSIE_STEAMER_COMMON_TIME_H

#include <chrono>

#include "absl/types/optional.h"

namespace jessie_steamer {
namespace common {

class Timer {
 public:
  Timer() : frame_count_{0} {
    launch_time_ = last_second_time_ = last_frame_time_ = Now();
  }

  absl::optional<int> frame_rate() {
    ++frame_count_;
    last_frame_time_ = Now();
    absl::optional<int> ret = absl::nullopt;
    if (TimeInterval(last_second_time_, last_frame_time_) >= 1.0f) {
      last_second_time_ = last_frame_time_;
      ret.emplace(frame_count_);
      frame_count_ = 0;
    }
    return ret;
  }

  float time_from_launch() const {
    return TimeInterval(launch_time_, Now());
  }

  float time_from_last_frame() const {
    return TimeInterval(last_frame_time_, Now());
  }

 private:
  using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

  static TimePoint Now() { return std::chrono::high_resolution_clock::now(); }

  static float TimeInterval(const TimePoint& t1, const TimePoint& t2) {
    return std::chrono::duration<
        float, std::chrono::seconds::period>(t2 - t1).count();
  }

  TimePoint launch_time_, last_second_time_, last_frame_time_;
  int frame_count_;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_TIME_H */
