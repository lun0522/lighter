//
//  util.h
//
//  Created by Pujun Lun on 10/22/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_UTIL_H
#define LIGHTER_RENDERER_VK_UTIL_H

#include <functional>
#include <string>
#include <vector>

#include "lighter/common/image.h"
#include "lighter/common/util.h"
#include "third_party/absl/container/flat_hash_set.h"
#include "third_party/absl/functional/function_ref.h"
#include "third_party/absl/strings/str_cat.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/strings/string_view.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

#define CONTAINER_SIZE(container) static_cast<uint32_t>(container.size())

#define ASSERT_SUCCESS(event, error)                          \
  ASSERT_TRUE(event == VK_SUCCESS,                            \
              absl::StrFormat("Errno %d: %s", event, error))

namespace lighter {
namespace renderer {
namespace vk {
namespace util {

// Returns a function pointer to a Vulkan instance function, and throws a
// runtime exception if it does not exist.
template<typename FuncType>
FuncType LoadInstanceFunction(const VkInstance& instance,
                              absl::string_view function_name) {
  auto func = reinterpret_cast<FuncType>(
      vkGetInstanceProcAddr(instance, function_name.data()));
  ASSERT_NON_NULL(
      func, absl::StrCat("Failed to load instance function: ", function_name));
  return func;
}

// Returns a function pointer to a Vulkan device function, and throws a runtime
// exception if it does not exist.
template<typename FuncType>
FuncType LoadDeviceFunction(const VkDevice& device,
                            absl::string_view function_name) {
  auto func = reinterpret_cast<FuncType>(
      vkGetDeviceProcAddr(device, function_name.data()));
  ASSERT_NON_NULL(
      func, absl::StrCat("Failed to load device function: ", function_name));
  return func;
}

// Queries attributes using the given enumerator. This is usually used with
// functions prefixed with 'vkGet' or 'vkEnumerate', which take in a uint32_t*
// to store the count, and a AttribType* to store query results.
template <typename AttribType>
std::vector<AttribType> QueryAttribute(
    absl::FunctionRef<void(uint32_t*, AttribType*)> enumerate) {
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
    absl::FunctionRef<const char*(const AttribType&)> get_name) {
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

// Creates VkExtent2D with given dimensions.
inline VkExtent2D CreateExtent(int width, int height) {
  return VkExtent2D{static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height)};
}

} /* namespace util */

constexpr uint32_t nullflag = 0;
constexpr uint32_t kSingleMipLevel = common::image::kSingleMipLevel;
constexpr uint32_t kSingleImageLayer = common::image::kSingleImageLayer;
constexpr uint32_t kCubemapImageLayer = common::image::kCubemapImageLayer;

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_UTIL_H */
