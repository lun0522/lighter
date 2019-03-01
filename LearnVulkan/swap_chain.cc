//
//  swap_chain.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "swap_chain.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_set>

#include "application.h"

using namespace std;

namespace vulkan {

namespace {

VkSurfaceFormatKHR ChooseSurfaceFormat(
  const vector<VkSurfaceFormatKHR>& available) {
  // if surface has no preferred format, we can choose any format
  if (available.size() == 1 && available[0].format == VK_FORMAT_UNDEFINED)
    return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

  for (auto candidate : available) {
    if (candidate.format == VK_FORMAT_B8G8R8A8_UNORM &&
        candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      return candidate;
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
  if (capabilities.currentExtent.width != numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    current_extent.width = std::max(capabilities.minImageExtent.width,
                                    current_extent.width);
    current_extent.width = std::min(capabilities.maxImageExtent.width,
                                    current_extent.width);
    current_extent.height = std::max(capabilities.minImageExtent.height,
                                     current_extent.height);
    current_extent.height = std::min(capabilities.maxImageExtent.height,
                                     current_extent.height);
    return current_extent;
  }
}

void CreateImages(vector<VkImage>* images,
                  vector<VkImageView>* image_views,
                  const VkSwapchainKHR& swap_chain,
                  const VkDevice& device,
                  VkFormat image_format) {
  // image count might be different since previously we only set a minimum
  *images = util::QueryAttribute<VkImage>(
    [&device, &swap_chain](uint32_t *count, VkImage *images) {
    vkGetSwapchainImagesKHR(device, swap_chain, count, images);
  });

  // use image view to specify how will we use these images
  // (color, depth, stencil, etc)
  image_views->resize(images->size());
  for (uint32_t i = 0; i < images->size(); ++i) {
    VkImageViewCreateInfo image_view_info{};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = (*images)[i];
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D; // 2D, 3D, cube maps
    image_view_info.format = image_format;
    // .components enables swizzling color channels around
    image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    // .subresourceRange specifies image's purpose and which part to access
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;

    ASSERT_SUCCESS(vkCreateImageView(
                     device, &image_view_info, nullptr, &(*image_views)[i]),
                   "Failed to create image view");
  }
}

} /* namespace */

bool SwapChain::HasSwapChainSupport(const VkSurfaceKHR& surface,
                                    const VkPhysicalDevice& physical_device) {
  try {
    cout << "Checking extension support required for swap chain..."
         << endl << endl;

    vector<string> required{
      kSwapChainExtensions.begin(),
      kSwapChainExtensions.end(),
    };
    auto extensions {util::QueryAttribute<VkExtensionProperties>(
      [&physical_device]
      (uint32_t* count, VkExtensionProperties* properties) {
        return vkEnumerateDeviceExtensionProperties(
          physical_device, nullptr, count, properties);
      })
    };
    auto get_name = [](const VkExtensionProperties& property) -> const char* {
      return property.extensionName;
    };
    util::CheckSupport<VkExtensionProperties>(required, extensions, get_name);
  } catch (const exception& e) {
    return false;
  }

  // physical device may support swap chain but maybe not compatible with
  // window system, so we need to query details
  uint32_t format_count, mode_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(
    physical_device, surface, &format_count, nullptr);
  vkGetPhysicalDeviceSurfacePresentModesKHR(
    physical_device, surface, &mode_count, nullptr);
  return format_count && mode_count;
}

void SwapChain::Init() {
  const VkSurfaceKHR& surface = *app_.surface();
  const VkPhysicalDevice& physical_device = *app_.physical_device();
  const VkDevice& device = *app_.device();
  const Queues& queues = app_.queues();

  // surface capabilities
  VkSurfaceCapabilitiesKHR surface_capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    physical_device, surface, &surface_capabilities);
  VkExtent2D extent = ChooseExtent(surface_capabilities, app_.current_extent());

  // surface formats
  auto surface_formats{util::QueryAttribute<VkSurfaceFormatKHR>(
    [&surface, &physical_device](uint32_t* count, VkSurfaceFormatKHR* formats) {
      return vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device, surface, count, formats);
    })
  };
  VkSurfaceFormatKHR surface_format = ChooseSurfaceFormat(surface_formats);

  // present modes
  auto present_modes{util::QueryAttribute<VkPresentModeKHR>(
    [&surface, &physical_device](uint32_t* count, VkPresentModeKHR* modes) {
      return vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device, surface, count, modes);
    })
  };
  VkPresentModeKHR presentMode = ChoosePresentMode(present_modes);

  // how many images we want to have in swap chain
  uint32_t image_count = surface_capabilities.minImageCount + 1;
  if (surface_capabilities.maxImageCount > 0) // can be 0 if no maximum
    image_count = std::min(surface_capabilities.maxImageCount, image_count);

  VkSwapchainCreateInfoKHR swap_chain_info{};
  swap_chain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swap_chain_info.surface = surface;
  swap_chain_info.minImageCount = image_count;
  swap_chain_info.imageFormat = surface_format.format;
  swap_chain_info.imageColorSpace = surface_format.colorSpace;
  swap_chain_info.imageExtent = extent;
  swap_chain_info.imageArrayLayers = 1;
  // .imageUsage can be different for post-processing
  swap_chain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  // we may apply transformations
  swap_chain_info.preTransform = surface_capabilities.currentTransform;
  // we may change alpha channel
  swap_chain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swap_chain_info.presentMode = presentMode;
  // don't care about color of pixels obscured
  swap_chain_info.clipped = VK_TRUE;
  swap_chain_info.oldSwapchain = VK_NULL_HANDLE;

  // graphics queue and present queue might be the same
  unordered_set<uint32_t> queue_families{
    queues.graphics.family_index,
    queues.present.family_index,
  };
  if (queue_families.size() == 1) {
    // if only one queue family will access this swap chain
    swap_chain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  } else {
    // specify which queue families will share access to images
    // we will draw on images in swap chain from graphics queue
    // and submit on presentation queue
    vector<uint32_t> indices{queue_families.begin(), queue_families.end()};
    swap_chain_info.queueFamilyIndexCount = CONTAINER_SIZE(indices);
    swap_chain_info.pQueueFamilyIndices = indices.data();
    swap_chain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
  }

  ASSERT_SUCCESS(vkCreateSwapchainKHR(
                   device, &swap_chain_info, nullptr, &swap_chain_),
                 "Failed to create swap chain");

  image_format_ = surface_format.format;
  image_extent_ = extent;
  CreateImages(&images_, &image_views_, swap_chain_, device, image_format_);
}

void SwapChain::Cleanup() {
  // images are implicitly cleaned up with swap chain
  const VkDevice& device = *app_.device();
  for (auto& image_view : image_views_)
    vkDestroyImageView(device, image_view, nullptr);
  vkDestroySwapchainKHR(device, swap_chain_, nullptr);
}

const vector<const char*> kSwapChainExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

} /* namespace vulkan */
