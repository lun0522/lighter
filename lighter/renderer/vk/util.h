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
#include "third_party/absl/strings/str_format.h"
#include "third_party/glm/glm.hpp"

// All members of Vulkan-Hpp are placed in the intl namespace.
#define VULKAN_HPP_NAMESPACE lighter::renderer::vk::intl
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "third_party/vulkan/vulkan.hpp"

#define CAST_TO_UINT(number) static_cast<uint32_t>(number)

#define CONTAINER_SIZE(container) CAST_TO_UINT((container).size())

#define ASSERT_SUCCESS(result, error)                               \
    ASSERT_TRUE(                                                    \
        result == ::lighter::renderer::vk::intl::Result::eSuccess,  \
        ::absl::StrFormat(                                          \
            "%s: %s", error, ::lighter::renderer::vk::intl::to_string(result)))

namespace lighter::renderer::vk::util {

// Creates VkExtent2D with given dimensions.
inline intl::Extent2D CreateExtent(int width, int height) {
  return {CAST_TO_UINT(width), CAST_TO_UINT(height)};
}

// Conversion between glm::ivec2 and VkExtent2D.
inline intl::Extent2D ToExtent(const glm::ivec2& vec) {
  return CreateExtent(vec.x, vec.y);
}
inline glm::ivec2 ToVec(const intl::Extent2D& extent) {
  return {static_cast<int>(extent.width), static_cast<int>(extent.height)};
}

// Conversion between glm::ivec2 and VkOffset2D.
inline intl::Offset2D ToOffset(const glm::ivec2& vec) {
  return {vec.x, vec.y};
}

}  // namespace lighter::renderer::vk::util

#endif  // LIGHTER_RENDERER_VK_UTIL_H
