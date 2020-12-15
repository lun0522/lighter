//
//  swapchain.cc
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/swapchain.h"

#include <algorithm>
#include <limits>

#include "lighter/common/image.h"
#include "lighter/renderer/vulkan/wrapper/util.h"
#include "third_party/absl/memory/memory.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

// Returns the surface format to use.
VkSurfaceFormatKHR ChooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& available) {
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
    const std::vector<VkPresentModeKHR>& available) {
  // FIFO mode is guaranteed to be available, but not properly supported by
  // some drivers. we will prefer MAILBOX and IMMEDIATE mode over it.
  VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

  for (const auto candidate : available) {
    switch (candidate) {
      case VK_PRESENT_MODE_MAILBOX_KHR:
        return candidate;
      case VK_PRESENT_MODE_IMMEDIATE_KHR:
        best_mode = candidate;
        break;
      default:
        break;
    }
  }

  return best_mode;
}

// Returns the image extent to use.
VkExtent2D ChooseImageExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                             const VkExtent2D& frame_size) {
  // 'currentExtent' is the suggested resolution.
  // If it is UINT32_MAX, that means it is up to the swapchain to choose extent.
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    VkExtent2D extent = frame_size;
    extent.width = std::max(extent.width, capabilities.minImageExtent.width);
    extent.width = std::min(extent.width, capabilities.maxImageExtent.width);
    extent.height = std::max(extent.height, capabilities.minImageExtent.height);
    extent.height = std::min(extent.height, capabilities.maxImageExtent.height);
    return extent;
  }
}

// Returns the minimum number of images we want to have in the swapchain.
// Note that the actual number can be higher.
uint32_t ChooseMinImageCount(const VkSurfaceCapabilitiesKHR& capabilities) {
  uint32_t min_count = capabilities.minImageCount + 1;
  // If there is no maximum limit, 'maxImageCount' will be 0.
  if (capabilities.maxImageCount > 0) {
    min_count = std::min(capabilities.maxImageCount, min_count);
  }
  return min_count;
}

} /* namespace */

VkSurfaceCapabilitiesKHR Surface::GetCapabilities() const {
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      *context_->physical_device(), surface_, &capabilities);
  return capabilities;
}

Surface::~Surface() {
  if (is_initialized_) {
    vkDestroySurfaceKHR(*context_->instance(), surface_,
                        *context_->allocator());
  }
}

Swapchain::Swapchain(
    SharedBasicContext context,
    const Surface& surface, const VkExtent2D& frame_size,
    absl::optional<MultisampleImage::Mode> multisampling_mode)
    : context_{std::move(FATAL_IF_NULL(context))} {
  // Choose image extent.
  const auto surface_capabilities = surface.GetCapabilities();
  image_extent_ = ChooseImageExtent(surface_capabilities, frame_size);

  // Choose surface format.
  const auto surface_formats{util::QueryAttribute<VkSurfaceFormatKHR>(
      [this, &surface]
      (uint32_t* count, VkSurfaceFormatKHR* formats) {
        return vkGetPhysicalDeviceSurfaceFormatsKHR(
            *context_->physical_device(), *surface, count, formats);
      }
  )};
  const VkSurfaceFormatKHR surface_format =
      ChooseSurfaceFormat(surface_formats);

  // Choose present mode.
  const auto present_modes{util::QueryAttribute<VkPresentModeKHR>(
      [this, &surface](uint32_t* count, VkPresentModeKHR* modes) {
        return vkGetPhysicalDeviceSurfacePresentModesKHR(
            *context_->physical_device(), *surface, count, modes);
      }
  )};
  const VkPresentModeKHR present_mode = ChoosePresentMode(present_modes);

  const util::QueueUsage queue_usage{{
      context_->queues().graphics_queue().family_index,
      context_->queues().transfer_queue().family_index,
      context_->queues().present_queue().family_index,
  }};

  const VkSwapchainCreateInfoKHR swapchain_info{
      VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      *surface,
      ChooseMinImageCount(surface_capabilities),
      surface_format.format,
      surface_format.colorSpace,
      image_extent_,
      common::image::kSingleImageLayer,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      queue_usage.sharing_mode(),
      queue_usage.unique_family_indices_count(),
      queue_usage.unique_family_indices(),
      // May apply transformations.
      /*preTransform=*/surface_capabilities.currentTransform,
      // May alter the alpha channel.
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      present_mode,
      // If true, we don't care about the color of invisible pixels.
      /*clipped=*/VK_TRUE,
      /*oldSwapchain=*/VK_NULL_HANDLE,
  };

  ASSERT_SUCCESS(vkCreateSwapchainKHR(*context_->device(), &swapchain_info,
                                      *context_->allocator(), &swapchain_),
                 "Failed to create swapchain");

  // Fetch swapchain images.
  const auto images = util::QueryAttribute<VkImage>(
      [this](uint32_t* count, VkImage* images) {
        vkGetSwapchainImagesKHR(*context_->device(), swapchain_, count, images);
      }
  );
  swapchain_images_.reserve(images.size());
  for (const auto& image : images) {
    swapchain_images_.push_back(absl::make_unique<SwapchainImage>(
        context_, image, image_extent_, surface_format.format));
  }

  // Create a multisample image if multisampling is enabled.
  if (multisampling_mode.has_value()) {
    multisample_image_ = MultisampleImage::CreateColorMultisampleImage(
        context_, *swapchain_images_[0], multisampling_mode.value());
  }
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
                        *context_->allocator());
#ifndef NDEBUG
  LOG_INFO << "Swapchain destructed";
#endif  /* !NDEBUG */
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
