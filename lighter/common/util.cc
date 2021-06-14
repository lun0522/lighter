//
//  util.cc
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/common/util.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace lighter::common::util {

std::string GetCurrentTime() {
  using namespace std::chrono;

  const auto now = system_clock::now();
  const auto now_t = system_clock::to_time_t(now);
  const std::tm now_tm = *std::localtime(&now_t);
  const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  std::stringstream stream;
  stream << std::put_time(&now_tm, "%F %T")
         << absl::StreamFormat(".%03d", ms.count());
  return stream.str();
}

}  // namespace lighter::common::util
