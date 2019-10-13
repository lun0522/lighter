//
//  util.cc
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/util.h"

#include <chrono>
#include <ctime>
#include <iomanip>

namespace jessie_steamer {
namespace common {
namespace util {

std::ostream& PrintTime(std::ostream& os) {
  using namespace std::chrono;

  // Print the "YYYY-MM-DD HH:MM:SS" part.
  const auto now = system_clock::now();
  const auto now_t = system_clock::to_time_t(now);
  const std::tm now_tm = *std::localtime(&now_t);
  os << std::put_time(&now_tm, "%F %T");

  // Print the ".fff" part.
  const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
  os << absl::StreamFormat(".%03d", ms.count());

  return os;
}

} /* namespace util */
} /* namespace common */
} /* namespace jessie_steamer */
