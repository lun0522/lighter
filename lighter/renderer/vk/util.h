#ifndef LIGHTER_RENDERER_VK_UTIL_H
#define LIGHTER_RENDERER_VK_UTIL_H

#include <format>
#include <glm/glm.hpp>
#include <string_view>
#include <vector>

#include "lighter/base/image.h"
#include "lighter/base/util.h"

// Keep Vulkan-Hpp in its default ::vk namespace (SDK 1.4.350 references
// vk::Result directly) and alias it as lighter::renderer::vk::intl.
// VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 is defined by CMake on vk targets so
// header include order never triggers DispatchLoaderStatic mismatch.
#include <vulkan/vulkan.hpp>

namespace lighter::renderer::vk {
namespace intl = ::vk;
}  // namespace lighter::renderer::vk

#define CAST_TO_UINT(number) static_cast<uint32_t>(number)

#define CONTAINER_SIZE(container) CAST_TO_UINT((container).size())

#define ASSERT_SUCCESS(result, error)                                    \
  ASSERT_TRUE(result == ::lighter::renderer::vk::intl::Result::eSuccess, \
              ::std::format("{}: {}", error,                             \
                            ::lighter::renderer::vk::intl::to_string(result)))

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

}  // namespace lighter::renderer::vk::util

#endif  // LIGHTER_RENDERER_VK_UTIL_H
