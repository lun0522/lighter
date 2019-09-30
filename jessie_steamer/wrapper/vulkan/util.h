//
//  util.h
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_UTIL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_UTIL_H

#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/types/optional.h"
#include "jessie_steamer/common/util.h"
#include "third_party/vulkan/vulkan.h"

#define ASSERT_SUCCESS(event, error)                          \
  ASSERT_TRUE(event == VK_SUCCESS,                            \
              absl::StrFormat("Errno %d: %s", event, error))

#define CONTAINER_SIZE(container) static_cast<uint32_t>(container.size())

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace util {

class QueueUsage {
 public:
  explicit QueueUsage(std::vector<uint32_t>&& queue_family_indices) {
    ASSERT_FALSE(queue_family_indices.empty(),
                 "Must contain at least one queue");
    common::util::RemoveDuplicate(&queue_family_indices);
    unique_family_indices_ = std::move(queue_family_indices);
    sharing_mode_ = unique_family_indices_.size() == 1 ?
        VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
  }

  // Accessors.
  const uint32_t* unique_family_indices() const {
    return unique_family_indices_.data();
  }
  uint32_t unique_family_indices_count() const {
    return CONTAINER_SIZE(unique_family_indices_);
  }
  VkSharingMode sharing_mode() const { return sharing_mode_; }

 private:
  std::vector<uint32_t> unique_family_indices_;
  VkSharingMode sharing_mode_;
};

template<typename FuncType>
FuncType LoadInstanceFunction(const VkInstance& instance,
                              const std::string& func_name) {
  auto func = reinterpret_cast<FuncType>(
      vkGetInstanceProcAddr(instance, func_name.c_str()));
  ASSERT_NON_NULL(
      func, absl::StrCat("Failed to load instance function ", func_name));
  return func;
}

template<typename FuncType>
FuncType LoadDeviceFunction(const VkDevice& device,
                            const std::string& func_name) {
  auto func = reinterpret_cast<FuncType>(
      vkGetDeviceProcAddr(device, func_name.c_str()));
  ASSERT_NON_NULL(
      func, absl::StrCat("Failed to load device function ", func_name));
  return func;
}

// Queries attributes using the given enumerator. This is usually used with
// functions prefixed with 'vkGet' or 'vkEnumerate', which take in a uint32_t*
// to store the count, and a AttribType* to store query results.
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

inline float GetWidthHeightRatio(VkExtent2D extent) {
  return static_cast<float>(extent.width) / extent.height;
}

} /* namespace util */

constexpr uint32_t nullflag = 0;

constexpr uint32_t kPerVertexBindingPoint = 0;
constexpr uint32_t kPerInstanceBindingPointBase = 1;

constexpr int kCubemapImageCount = 6;

constexpr uint32_t kSingleMipLevel = 1;
constexpr uint32_t kSingleImageLayer = 1;
constexpr VkSampleCountFlagBits kSingleSample = VK_SAMPLE_COUNT_1_BIT;

constexpr VkAccessFlags kNullAccessFlag = 0;
constexpr uint32_t kExternalSubpassIndex = VK_SUBPASS_EXTERNAL;

// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap36.html#limits-minmax
constexpr int kMaxPushConstantSize = 128;

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_UTIL_H */
