//
//  swapchain.cc
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/swapchain.h"

#include "lighter/common/image.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/vk/image_util.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/container/flat_hash_set.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/span.h"

namespace lighter {
namespace renderer {
namespace vk {
namespace {

// Returns the image extent to use.
VkExtent2D ChooseImageExtent(const common::Window& window,
                             const Surface& surface) {
  // 'currentExtent' is the suggested resolution.
  // If it is UINT32_MAX, that means it is up to the swapchain to choose extent.
  const auto& capabilities = surface.capabilities();
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    VkExtent2D extent = util::CreateExtent(window.GetFrameSize());
    extent.width = std::max(extent.width, capabilities.minImageExtent.width);
    extent.width = std::min(extent.width, capabilities.maxImageExtent.width);
    extent.height = std::max(extent.height, capabilities.minImageExtent.height);
    extent.height = std::min(extent.height, capabilities.maxImageExtent.height);
    return extent;
  }
}

// Returns the surface format to use.
VkSurfaceFormatKHR ChooseSurfaceFormat(
    absl::Span<const VkSurfaceFormatKHR> available) {
  constexpr VkSurfaceFormatKHR best_format{
      VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

  // If the surface has no preferred format, we can choose any format.
  if (available.size() == 1 && available[0].format == VK_FORMAT_UNDEFINED) {
    return best_format;
  }

  // Check whether our preferred format is available. If not, simply choose the
  // first available one.
  for (const auto& candidate : available) {
    if (candidate.format == best_format.format &&
        candidate.colorSpace == best_format.colorSpace) {
      return best_format;
    }
  }
  return available[0];
}

// Returns the present mode to use.
VkPresentModeKHR ChoosePresentMode(
    absl::Span<const VkPresentModeKHR> available) {
  // In FIFO mode, which is supported by all drivers, rendered images will wait
  // in a queue to be presented, while in MAILBOX mode, there will be only one
  // image waiting to be presented. If that image has not been presented yet and
  // GPU has finished rendering a new image, it will be replaced by the new one,
  // so that we always get the most recently generated frame.
  // TODO: Use FIFO for mobile to save power.
  constexpr auto kBestMode = VK_PRESENT_MODE_MAILBOX_KHR;
  const auto iter = std::find(available.begin(), available.end(), kBestMode);
  return iter == available.end() ? VK_PRESENT_MODE_FIFO_KHR : kBestMode;
}

// Returns the minimum number of images we want to have in the swapchain.
// Note that the actual number can be higher.
uint32_t ChooseMinImageCount(const Surface& surface) {
  const auto& capabilities = surface.capabilities();
  // Prefer triple-buffering.
  uint32_t min_count = 3;
  min_count = std::max(min_count, capabilities.minImageCount);
  // If there is no maximum limit, 'maxImageCount' will be 0.
  if (capabilities.maxImageCount > 0) {
    min_count = std::min(min_count, capabilities.maxImageCount);
  }
  return min_count;
}

} /* namespace */

Swapchain::Swapchain(SharedContext context, int window_index,
                     const common::Window& window)
    : context_{std::move(FATAL_IF_NULL(context))} {
  // Choose image extent.
  const Surface& surface = context_->surface(window_index);
  const VkExtent2D image_extent = ChooseImageExtent(window, surface);

  // Choose surface format.
  const auto surface_formats{util::QueryAttribute<VkSurfaceFormatKHR>(
      [this, &surface] (uint32_t* count, VkSurfaceFormatKHR* formats) {
        return vkGetPhysicalDeviceSurfaceFormatsKHR(
            *context_->physical_device(), *surface, count, formats);
      }
  )};
  const auto surface_format = ChooseSurfaceFormat(surface_formats);

  // Choose present mode.
  const auto present_modes{util::QueryAttribute<VkPresentModeKHR>(
      [this, &surface](uint32_t* count, VkPresentModeKHR* modes) {
        return vkGetPhysicalDeviceSurfacePresentModesKHR(
            *context_->physical_device(), *surface, count, modes);
      }
  )};
  const auto present_mode = ChoosePresentMode(present_modes);

  // For swapchain images, we don't expect complicated operations, but being
  // rendered to (or resolved to) and then presented to screen.
  // Arbitrary 'attachment_location' would work for image creation.
  const std::vector<ImageUsage> swapchain_image_usages{
      ImageUsage::GetRenderTargetUsage(/*attachment_location=*/0),
      ImageUsage::GetMultisampleResolveTargetUsage(),
      ImageUsage::GetPresentationUsage()};

  // Only graphics queue and presentation queue would access swapchain images.
  const auto& queue_family_indices =
      context_->physical_device().queue_family_indices();
  const absl::flat_hash_set<uint32_t> queue_family_indices_set{
      queue_family_indices.graphics,
      queue_family_indices.presents.at(window_index),
  };
  const std::vector<uint32_t> unique_queue_family_indices{
      queue_family_indices_set.begin(),
      queue_family_indices_set.end(),
  };

  const VkSwapchainCreateInfoKHR swapchain_info{
      VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      *surface,
      ChooseMinImageCount(surface),
      surface_format.format,
      surface_format.colorSpace,
      image_extent,
      kSingleImageLayer,
      image::GetImageUsageFlags(swapchain_image_usages),
      VK_SHARING_MODE_EXCLUSIVE,
      CONTAINER_SIZE(unique_queue_family_indices),
      unique_queue_family_indices.data(),
      // May apply transformations.
      /*preTransform=*/surface.capabilities().currentTransform,
      // May alter the alpha channel.
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      present_mode,
      // If true, we don't care about the color of invisible pixels.
      /*clipped=*/VK_TRUE,
      /*oldSwapchain=*/VK_NULL_HANDLE,
  };

  ASSERT_SUCCESS(vkCreateSwapchainKHR(*context_->device(), &swapchain_info,
                                      *context_->host_allocator(), &swapchain_),
                 "Failed to create swapchain");

  // Fetch swapchain images.
  image_ = absl::make_unique<SwapchainImage>(
      absl::StrFormat("Swapchain%d", window_index),
      util::QueryAttribute<VkImage>(
          [this](uint32_t* count, VkImage* images) {
            vkGetSwapchainImagesKHR(*context_->device(), swapchain_, count,
                                    images);
          }));
}

const std::vector<const char*>& Swapchain::GetRequiredExtensions() {
  static const std::vector<const char*>* required_extensions = nullptr;
  if (required_extensions == nullptr) {
    required_extensions = new std::vector<const char*>{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
  }
  return *required_extensions;
}

Swapchain::~Swapchain() {
  vkDestroySwapchainKHR(*context_->device(), swapchain_,
                        *context_->host_allocator());
#ifndef NDEBUG
  LOG_INFO << "Swapchain destructed";
#endif  /* !NDEBUG */
}

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
