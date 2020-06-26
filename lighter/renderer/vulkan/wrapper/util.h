//
//  util.h
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_WRAPPER_UTIL_H
#define LIGHTER_RENDERER_VULKAN_WRAPPER_UTIL_H

#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "lighter/common/util.h"
#include "third_party/absl/container/flat_hash_set.h"
#include "third_party/absl/strings/str_cat.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

#define ASSERT_SUCCESS(event, error)                          \
  ASSERT_TRUE(event == VK_SUCCESS,                            \
              absl::StrFormat("Errno %d: %s", event, error))

#define CONTAINER_SIZE(container) static_cast<uint32_t>(container.size())

namespace lighter {
namespace renderer {
namespace vulkan {
namespace util {

// This class is used to extract unique queues from a list of queues that may
// contain duplicates. Since we may use one device queue for different purposes,
// such as graphics and presentation, we need to know how many unique queues are
// actually used. If there is only one unique queue, resources will not be
// shared across queues.
class QueueUsage {
 public:
  explicit QueueUsage(std::vector<uint32_t>&& queue_family_indices);

  // This class is only movable.
  QueueUsage(QueueUsage&&) noexcept = default;
  QueueUsage& operator=(QueueUsage&&) noexcept = default;

  // Accessors.
  const uint32_t* unique_family_indices() const {
    return unique_family_indices_.data();
  }
  uint32_t unique_family_indices_count() const {
    return CONTAINER_SIZE(unique_family_indices_);
  }
  VkSharingMode sharing_mode() const { return sharing_mode_; }

 private:
  // Family indices of unique queues.
  std::vector<uint32_t> unique_family_indices_;

  // Whether resources will be shared by multiple queues.
  VkSharingMode sharing_mode_;
};

// Returns a function pointer to a Vulkan instance function, and throws a
// runtime exception if it does not exist.
template<typename FuncType>
FuncType LoadInstanceFunction(const VkInstance& instance,
                              const std::string& func_name) {
  auto func = reinterpret_cast<FuncType>(
      vkGetInstanceProcAddr(instance, func_name.c_str()));
  ASSERT_NON_NULL(
      func, absl::StrCat("Failed to load instance function ", func_name));
  return func;
}

// Returns a function pointer to a Vulkan device function, and throws a runtime
// exception if it does not exist.
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

// Checks whether 'attribs' covers 'required' attributes. If not, returns the
// name of the first uncovered attribute. 'get_name' should be able to return
// the name of any attribute of AttribType.
template <typename AttribType>
absl::optional<std::string> FindUnsupported(
    absl::Span<const std::string> required,
    absl::Span<const AttribType> attribs,
    const std::function<const char*(const AttribType&)>& get_name) {
  absl::flat_hash_set<std::string> available{attribs.size()};
  for (const auto& atr : attribs) {
    available.insert(get_name(atr));
  }

  LOG_INFO << "Available:";
  for (const auto& avl : available) {
    LOG_INFO << "\t" << avl;
  }
  LOG_EMPTY_LINE;

  LOG_INFO << "Required:";
  for (const auto& req : required) {
    LOG_INFO << "\t" << req;
  }
  LOG_EMPTY_LINE;

  for (const auto& req : required) {
    if (!available.contains(req)) {
      return req;
    }
  }
  return absl::nullopt;
}

// Return the number of work groups in one dimension used for compute shader.
inline uint32_t GetWorkGroupCount(int total_size, int work_group_size) {
  return static_cast<uint32_t>(std::ceil(
      static_cast<float>(total_size) / work_group_size));
}

// Return the number of work groups used for compute shader.
inline VkExtent2D GetWorkGroupCount(const VkExtent2D& total_size,
                                    const VkExtent2D& work_group_size) {
  return {GetWorkGroupCount(total_size.width, work_group_size.width),
          GetWorkGroupCount(total_size.height, work_group_size.height)};
}

// Returns the aspect ratio of the 2D 'extent'.
inline float GetAspectRatio(const VkExtent2D& extent) {
  return static_cast<float>(extent.width) / extent.height;
}

// Converts a VkExtent2D to glm::vec2.
inline glm::vec2 ExtentToVec(const VkExtent2D& extent) {
  return glm::vec2{extent.width, extent.height};
}

// Converts a bool to VkBool32.
inline VkBool32 ToVkBool(bool value) { return value ? VK_TRUE : VK_FALSE; }

// Returns the index of a VkMemoryType that satisfies both 'memory_type' and
// 'memory_properties' within VkPhysicalDeviceMemoryProperties.memoryTypes.
uint32_t FindMemoryTypeIndex(const VkPhysicalDevice& physical_device,
                             uint32_t memory_type,
                             VkMemoryPropertyFlags memory_properties);

} /* namespace util */

constexpr uint32_t nullflag = 0;

constexpr uint32_t kSingleMipLevel = 1;
constexpr uint32_t kSingleImageLayer = 1;
constexpr VkSampleCountFlagBits kSingleSample = VK_SAMPLE_COUNT_1_BIT;

constexpr VkAccessFlags kNullAccessFlag = 0;
constexpr uint32_t kExternalSubpassIndex = VK_SUBPASS_EXTERNAL;

constexpr VkMemoryPropertyFlags kHostVisibleMemory =
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap36.html#limits-minmax
constexpr int kMaxPushConstantSize = 128;

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_WRAPPER_UTIL_H */
