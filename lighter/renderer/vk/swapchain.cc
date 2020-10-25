//
//  swapchain.cc
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/swapchain.h"

#include "lighter/common/image.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"

namespace lighter {
namespace renderer {
namespace vk {
namespace {

// Returns the image extent to use.
VkExtent2D ChooseImageExtent(const WindowContext& window_context) {
  // 'currentExtent' is the suggested resolution.
  // If it is UINT32_MAX, that means it is up to the swapchain to choose extent.
  const auto& capabilities = window_context.surface().capabilities();
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    const glm::ivec2 frame_size = window_context.window().GetFrameSize();
    VkExtent2D extent = util::CreateExtent(frame_size.x, frame_size.y);
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
  // FIFO mode is guaranteed to be available, but not properly supported by
  // some drivers. we will prefer MAILBOX and IMMEDIATE mode over it.
  VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;
  for (auto candidate : available) {
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

// Returns the minimum number of images we want to have in the swapchain.
// Note that the actual number can be higher.
uint32_t ChooseMinImageCount(const Surface& surface) {
  const auto& capabilities = surface.capabilities();
  uint32_t min_count = capabilities.minImageCount + 1;
  // If there is no maximum limit, 'maxImageCount' will be 0.
  if (capabilities.maxImageCount > 0) {
    min_count = std::min(capabilities.maxImageCount, min_count);
  }
  return min_count;
}

} /* namespace */

Swapchain::Swapchain(SharedContext context, const WindowContext& window_context,
                     MultisamplingMode multisampling_mode)
    : context_{std::move(FATAL_IF_NULL(context))} {
  // Choose image extent.
  const VkExtent2D image_extent = ChooseImageExtent(window_context);

  // Choose surface format.
  const Surface& surface = window_context.surface();
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

  // Find out which queues access swapchain images.
  const auto& queue_family_indices =
      context_->physical_device().queue_family_indices();
  const util::QueueUsage queue_usage{{
      queue_family_indices.graphics,
      queue_family_indices.compute,
      queue_family_indices.presents.at(window_context.window_index()),
  }};
  const std::vector<uint32_t> unique_queue_family_indices =
      queue_usage.GetUniqueQueueFamilyIndices();

  // TODO: Pass in image usage and infer usage bit.
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
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      queue_usage.sharing_mode(),
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
  const auto images = util::QueryAttribute<VkImage>(
      [this](uint32_t* count, VkImage* images) {
        vkGetSwapchainImagesKHR(*context_->device(), swapchain_, count, images);
      }
  );
  swapchain_images_.reserve(images.size());
  for (auto& image : images) {
    swapchain_images_.push_back(
        absl::make_unique<DeviceImage>(context_, image));
  }

  // Create a multisample image if multisampling is enabled.
  if (multisampling_mode != MultisamplingMode::kNone) {
    // TODO: Pass in image usage.
    const auto usages = {ImageUsage::GetRenderTargetUsage()};
    multisample_image_ = absl::make_unique<DeviceImage>(
        context_, surface_format.format, image_extent, kSingleMipLevel,
        kSingleImageLayer, multisampling_mode, usages);
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
                        *context_->host_allocator());
#ifndef NDEBUG
  LOG_INFO << "Swapchain destructed";
#endif  /* !NDEBUG */
}

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
