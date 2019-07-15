//
//  swapchain.cc
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/swapchain.h"

#include <algorithm>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

namespace util = common::util;

using std::max;
using std::min;
using std::vector;

VkSurfaceFormatKHR ChooseSurfaceFormat(
    const vector<VkSurfaceFormatKHR>& available) {
  // if surface has no preferred format, we can choose any format
  if (available.size() == 1 && available[0].format == VK_FORMAT_UNDEFINED) {
    return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  }

  for (auto candidate : available) {
    if (candidate.format == VK_FORMAT_B8G8R8A8_UNORM &&
        candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return candidate;
    }
  }

  // if our preferred format is not supported, simply choose the first one
  return available[0];
}

VkPresentModeKHR ChoosePresentMode(const vector<VkPresentModeKHR>& available) {
  // FIFO mode is guaranteed to be available, but not properly supported by
  // some drivers. we will prefer MAILBOX and IMMEDIATE mode over it
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

VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                        VkExtent2D current_extent) {
  // .currentExtent is the suggested resolution
  // if it is UINT32_MAX, window manager suggests us to be flexible
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    current_extent.width = max(capabilities.minImageExtent.width,
                               current_extent.width);
    current_extent.width = min(capabilities.maxImageExtent.width,
                               current_extent.width);
    current_extent.height = max(capabilities.minImageExtent.height,
                                current_extent.height);
    current_extent.height = min(capabilities.maxImageExtent.height,
                                current_extent.height);
    return current_extent;
  }
}

} /* namespace */

const vector<const char*>& Swapchain::required_extensions() {
  static vector<const char*>* kSwapchainExtensions = nullptr;
  if (kSwapchainExtensions == nullptr) {
    kSwapchainExtensions = new vector<const char*>{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
  }
  return *kSwapchainExtensions;
}

void Swapchain::Init(SharedBasicContext context,
                     const VkSurfaceKHR& surface, VkExtent2D screen_size) {
  context_ = std::move(context);
  const VkPhysicalDevice& physical_device = *context_->physical_device();

  // surface capabilities
  VkSurfaceCapabilitiesKHR surface_capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface,
                                            &surface_capabilities);
  VkExtent2D image_extent = ChooseExtent(surface_capabilities, screen_size);

  // surface formats
  auto surface_formats{util::QueryAttribute<VkSurfaceFormatKHR>(
      [&surface, &physical_device]
      (uint32_t* count, VkSurfaceFormatKHR* formats) {
        return vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device, surface, count, formats);
      }
  )};
  VkSurfaceFormatKHR surface_format = ChooseSurfaceFormat(surface_formats);

  // present modes
  auto present_modes{util::QueryAttribute<VkPresentModeKHR>(
      [&surface, &physical_device](uint32_t* count, VkPresentModeKHR* modes) {
        return vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device, surface, count, modes);
      }
  )};
  VkPresentModeKHR present_mode = ChoosePresentMode(present_modes);

  // minimum amount of images we want to have in swapchain
  uint32_t min_image_count = surface_capabilities.minImageCount + 1;
  if (surface_capabilities.maxImageCount > 0) {  // can be 0 if no maximum
    min_image_count = min(surface_capabilities.maxImageCount, min_image_count);
  }

  VkSwapchainCreateInfoKHR swapchain_info{
      VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      surface,
      min_image_count,
      surface_format.format,
      surface_format.colorSpace,
      image_extent,
      /*imageArrayLayers=*/1,
      // can be different for post-processing
      /*imageUsage=*/VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      /*imageSharingMode=*/VK_SHARING_MODE_EXCLUSIVE,
      /*queueFamilyIndexCount=*/0,
      /*pQueueFamilyIndices=*/nullptr,
      // may apply transformations
      /*preTransform=*/surface_capabilities.currentTransform,
      // may change alpha channel
      /*compositeAlpha=*/VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      present_mode,
      /*clipped=*/VK_TRUE,  // don't care about color of pixels obscured
      /*oldSwapchain=*/VK_NULL_HANDLE,
  };

  // graphics queue and present queue might be the same
  const auto queue_family_indices = context_->queues().unique_family_indices();
  if (queue_family_indices.size() > 1) {
    // specify which queue families will share access to images. we will draw on
    // images in swapchain from graphics queue and submit on presentation queue.
    // otherwise set to VK_SHARING_MODE_EXCLUSIVE
    vector<uint32_t> indices{
        queue_family_indices.begin(),
        queue_family_indices.end(),
    };
    swapchain_info.queueFamilyIndexCount = CONTAINER_SIZE(indices);
    swapchain_info.pQueueFamilyIndices = indices.data();
    swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
  }

  ASSERT_SUCCESS(vkCreateSwapchainKHR(*context_->device(), &swapchain_info,
                                      context_->allocator(), &swapchain_),
                 "Failed to create swapchain");

  // fetch swapchain images and create image views for them
  auto images{util::QueryAttribute<VkImage>(
      [this](uint32_t *count, VkImage *images) {
        vkGetSwapchainImagesKHR(*context_->device(), swapchain_, count, images);
      }
  )};
  images_.reserve(images.size());
  for (const auto& image : images) {
    images_.emplace_back(absl::make_unique<SwapchainImage>(
        context_, image, surface_format.format));
  }
  image_extent_ = image_extent;
}

void Swapchain::Cleanup() {
  images_.clear();
  vkDestroySwapchainKHR(*context_->device(), swapchain_, context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
