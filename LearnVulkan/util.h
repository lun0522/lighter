//
//  util.h
//  LearnVulkan
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LEARNVULKAN_UTIL_H
#define LEARNVULKAN_UTIL_H

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#define ASSERT_NONNULL(object, error) \
    if (object == nullptr)      throw std::runtime_error{error}
#define ASSERT_SUCCESS(event, error) \
    if (event != VK_SUCCESS)    throw std::runtime_error{error}
#define CONTAINER_SIZE(container)   static_cast<uint32_t>(container.size())
#define MARK_NOT_COPYABLE_OR_MOVABLE(typename) \
        typename(const typename&) = delete; \
        typename& operator=(const typename&) = delete

namespace util {

template<typename AttribType>
std::vector<AttribType> QueryAttribute(
  const std::function<void (uint32_t*, AttribType*)>& enumerate) {
  uint32_t count;
  enumerate(&count, nullptr);
  std::vector<AttribType> attribs{count};
  enumerate(&count, attribs.data());
  return attribs;
}

template<typename AttribType>
void CheckSupport(
    const std::vector<std::string>& required,
    const std::vector<AttribType>& attribs,
    const std::function<const char* (const AttribType&)>& get_name) {
  std::unordered_set<std::string> available{attribs.size()};
  for (const auto& atr : attribs)
    available.insert(get_name(atr));
  
  std::cout << "Available:" << std::endl;
  for (const auto& avl : available)
    std::cout << "\t" << avl << std::endl;
  std::cout << std::endl;
  
  std::cout << "Required:" << std::endl;
  for (const auto& req : required)
    std::cout << "\t" << req << std::endl;
  std::cout << std::endl;
  
  for (const auto& req : required) {
    if (available.find(req) == available.end())
      throw std::runtime_error{"Requirement not satisfied: " + req};
  }
}

template <typename ContentType>
bool FindFirst(const std::vector<ContentType>& container,
               const std::function<bool (const ContentType&)>& predicate,
               uint32_t& first) {
  auto first_itr = find_if(container.begin(), container.end(), predicate);
  first = static_cast<uint32_t>(distance(container.begin(), first_itr));
  return first_itr != container.end();
}

const std::string& ReadFile(const std::string& path);

}  /* namespace util */

#endif /* LEARNVULKAN_UTIL_H */
