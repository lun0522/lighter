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

#define VULKAN_HPP_NAMESPACE lighter::renderer::vk::intl
#include "third_party/vulkan/vulkan.hpp"
#undef VULKAN_HPP_NAMESPACE

#define CAST_TO_UINT(number) static_cast<uint32_t>(number)

#define CONTAINER_SIZE(container) CAST_TO_UINT((container).size())

#define ASSERT_SUCCESS(result, error)                               \
    ASSERT_TRUE(                                                    \
        result == ::lighter::renderer::vk::intl::Result::eSuccess,  \
        ::absl::StrFormat(                                          \
            "%s: %s", error, ::lighter::renderer::vk::intl::to_string(result))

namespace lighter::renderer::vk {
namespace util {

// Returns a function pointer to a Vulkan instance function, and throws a
// runtime exception if it does not exist.
template<typename FuncType>
FuncType LoadInstanceFunction(intl::Instance instance, const char* func_name) {
  auto func = reinterpret_cast<FuncType>(instance.GetProcAddr(func_name));
  ASSERT_NON_NULL(
      func, absl::StrFormat("Failed to load instance function %s", func_name));
  return func;
}

// Returns a function pointer to a Vulkan device function, and throws a runtime
// exception if it does not exist.
template<typename FuncType>
FuncType LoadDeviceFunction(intl::Device device, const char* func_name) {
  auto func = reinterpret_cast<FuncType>(device.GetProcAddr(func_name));
  ASSERT_NON_NULL(
      func, absl::StrFormat("Failed to load device function %s", func_name));
  return func;
}

// Creates VkExtent2D with given dimensions.
inline intl::Extent2D CreateExtent(int width, int height) {
  return {CAST_TO_UINT(width), CAST_TO_UINT(height)};
}
inline intl::Extent2D CreateExtent(const glm::ivec2& dimension) {
  return CreateExtent(dimension.x, dimension.y);
}

// Creates VkOffset2D with given dimensions.
inline intl::Offset2D CreateOffset(const glm::ivec2& dimension) {
  return {dimension.x, dimension.y};
}

}  // namespace util

constexpr uint32_t nullflag = 0;
constexpr uint32_t kSingleMipLevel = common::image::kSingleMipLevel;
constexpr uint32_t kSingleImageLayer = common::image::kSingleImageLayer;
constexpr uint32_t kCubemapImageLayer = common::image::kCubemapImageLayer;

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_UTIL_H
