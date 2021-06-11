//
//  util.h
//
//  Created by Pujun Lun on 10/22/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_UTIL_H
#define LIGHTER_RENDERER_VK_UTIL_H

#include <string_view>
#include <vector>

#include "lighter/common/image.h"
#include "lighter/common/util.h"
#include "third_party/absl/functional/function_ref.h"
#include "third_party/absl/strings/str_cat.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

#define CAST_TO_UINT(number) static_cast<uint32_t>(number)

#define CONTAINER_SIZE(container) CAST_TO_UINT((container).size())

#define ASSERT_SUCCESS(event, error)                          \
  ASSERT_TRUE(event == VK_SUCCESS,                            \
              absl::StrFormat("Errno %d: %s", event, error))

namespace lighter::renderer::vk {
namespace util {

// Returns a function pointer to a Vulkan instance function, and throws a
// runtime exception if it does not exist.
template<typename FuncType>
FuncType LoadInstanceFunction(const VkInstance& instance,
                              std::string_view function_name) {
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
                            std::string_view function_name) {
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

// Converts a bool to VkBool32.
inline VkBool32 ToVkBool(bool value) { return value ? VK_TRUE : VK_FALSE; }

// Creates VkExtent2D with given dimensions.
inline VkExtent2D CreateExtent(int width, int height) {
  return {CAST_TO_UINT(width), CAST_TO_UINT(height)};
}
inline VkExtent2D CreateExtent(const glm::ivec2& dimension) {
  return CreateExtent(dimension.x, dimension.y);
}

// Creates VkOffset2D with given dimensions.
inline VkOffset2D CreateOffset(const glm::ivec2& dimension) {
  return {dimension.x, dimension.y};
}

}  // namespace util

constexpr uint32_t nullflag = 0;
constexpr uint32_t kSingleMipLevel = common::image::kSingleMipLevel;
constexpr uint32_t kSingleImageLayer = common::image::kSingleImageLayer;
constexpr uint32_t kCubemapImageLayer = common::image::kCubemapImageLayer;

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_UTIL_H
