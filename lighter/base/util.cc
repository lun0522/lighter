#include "lighter/base/util.h"

#include <chrono>
#include <ctime>
#include <format>
#include <iomanip>
#include <sstream>

namespace lighter::base::util {

std::string GetCurrentTime() {
  const auto now = std::chrono::system_clock::now();
  const auto now_t = std::chrono::system_clock::to_time_t(now);
  const std::tm now_tm = *std::localtime(&now_t);
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

  std::stringstream stream;
  stream << std::put_time(&now_tm, "%F %T")
         << std::format(".{:03d}", ms.count());
  return stream.str();
}

std::vector<std::string> StrSplit(std::string_view text, char delimiter,
                                  bool skip_empty) {
  std::vector<std::string> segments;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t pos = text.find(delimiter, start);
    const std::size_t end = pos == std::string_view::npos ? text.size() : pos;
    if (!skip_empty || end > start) {
      segments.emplace_back(text.substr(start, end - start));
    }
    if (pos == std::string_view::npos) {
      break;
    }
    start = pos + 1;
  }
  return segments;
}

}  // namespace lighter::base::util
