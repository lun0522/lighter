//
//  util.h
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_UTIL_H
#define JESSIE_STEAMER_COMMON_UTIL_H

#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"

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
void CheckSupport(
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
      throw std::runtime_error{"Requirement not satisfied: " + req};
    }
  }
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

} /* namespace util */
} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_UTIL_H */
