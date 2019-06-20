//
//  util.h
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_UTIL_H
#define JESSIE_STEAMER_COMMON_UTIL_H

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_format.h"
#include "absl/types/optional.h"

#ifdef NDEBUG
#define FATAL(error) throw std::runtime_error{error};
#else  /* !NDEBUG */
#define FATAL(error)                          \
  throw std::runtime_error{absl::StrFormat(   \
      "%s() in %s at line %d: %s",            \
      __func__, __FILE__, __LINE__, error)};
#endif /* NDEBUG */

#define ASSERT_HAS_VALUE(object, error)       \
  if (!object.has_value()) { FATAL(error); }

namespace jessie_steamer {
namespace common {
namespace util {

template <typename AttribType>
std::vector<AttribType> QueryAttribute(
    const std::function<void(uint32_t*, AttribType*)>& enumerate) {
  uint32_t count;
  enumerate(&count, nullptr);
  std::vector<AttribType> attribs{count};
  enumerate(&count, attribs.data());
  return attribs;
}

template <typename AttribType>
absl::optional<std::string> FindUnsupported(
    const std::vector<std::string>& required,
    const std::vector<AttribType>& attribs,
    const std::function<const char*(const AttribType&)>& get_name) {
  absl::flat_hash_set<std::string> available{attribs.size()};
  for (const auto& atr : attribs) {
    available.emplace(get_name(atr));
  }

  std::cout << "Available:" << std::endl;
  for (const auto& avl : available) {
    std::cout << "\t" << avl << std::endl;
  }
  std::cout << std::endl;

  std::cout << "Required:" << std::endl;
  for (const auto& req : required) {
    std::cout << "\t" << req << std::endl;
  }
  std::cout << std::endl;

  for (const auto& req : required) {
    if (available.find(req) == available.end()) {
      return req;
    }
  }
  return absl::nullopt;
}

constexpr int kInvalidIndex = -1;
template <typename ContentType>
int FindFirst(const std::vector<ContentType>& container,
              const std::function<bool(const ContentType&)>& predicate) {
  auto first_itr = find_if(container.begin(), container.end(), predicate);
  return first_itr == container.end()
      ? kInvalidIndex
      : static_cast<int>(std::distance(container.begin(), first_itr));
}

template <typename ContentType>
void SetElementWithResizing(std::vector<ContentType>* container,
                            int index, ContentType&& element) {
  if (index >= container->size()) {
    container->resize(index + 1);
  }
  (*container)[index] = std::move(element);
}

} /* namespace util */
} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_UTIL_H */
