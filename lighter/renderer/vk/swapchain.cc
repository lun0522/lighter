//
//  swapchain.cc
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/swapchain.h"

#include <vector>

#include "lighter/common/image.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/vk/image_util.h"
#include "third_party/absl/container/flat_hash_set.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer::vk {
namespace {

using ir::ImageUsage;

// Returns the image extent to use.
intl::Extent2D ChooseImageExtent(const common::Window& window,
                                 const Surface& surface) {
  // 'currentExtent' is the suggested resolution.
  // If it is UINT32_MAX, that means it is up to the swapchain to choose extent.
  const auto& capabilities = surface.capabilities();
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    intl::Extent2D extent = util::ToExtent(window.GetFrameSize());
    extent.width = std::max(extent.width, capabilities.minImageExtent.width);
    extent.width = std::min(extent.width, capabilities.maxImageExtent.width);
    extent.height = std::max(extent.height, capabilities.minImageExtent.height);
    extent.height = std::min(extent.height, capabilities.maxImageExtent.height);
    return extent;
  }
}

// Returns the surface format to use.
intl::SurfaceFormatKHR ChooseSurfaceFormat(
    absl::Span<const intl::SurfaceFormatKHR> formats) {
  constexpr intl::SurfaceFormatKHR best_format{
      intl::Format::eB8G8R8A8Unorm, intl::ColorSpaceKHR::eSrgbNonlinear};

  // If the surface has no preferred format, we can choose any format.
  if (formats.size() == 1 && formats[0].format == intl::Format::eUndefined) {
    return best_format;
  }

  // Check whether our preferred format is available. If not, simply choose the
  // first available one.
  for (const auto& candidate : formats) {
    if (candidate.format == best_format.format &&
        candidate.colorSpace == best_format.colorSpace) {
      return best_format;
    }
  }
  return formats[0];
}

// Returns the present mode to use.
intl::PresentModeKHR ChoosePresentMode(
    absl::Span<const intl::PresentModeKHR> modes) {
  // In FIFO mode, which is supported by all drivers, rendered images will wait
  // in a queue to be presented, while in MAILBOX mode, there will be only one
  // image waiting to be presented. If that image has not been presented yet and
  // GPU has finished rendering a new image, it will be replaced by the new one,
  // so that we always get the most recently generated frame.
  // TODO: Use FIFO for mobile to save power.
  constexpr auto kBestMode = intl::PresentModeKHR::eMailbox;
  const bool supports_best_mode = common::util::Contains(modes, kBestMode);
  return supports_best_mode ? kBestMode : intl::PresentModeKHR::eFifo;
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

}  // namespace

Swapchain::Swapchain(const SharedContext& context, int window_index,
                     const common::Window& window)
    : WithSharedContext{context} {
  // Choose image extent.
  const PhysicalDevice& physical_device = context_->physical_device();
  const Surface& surface = context_->surface(window_index);
  const intl::Extent2D image_extent = ChooseImageExtent(window, surface);

  // Choose surface format and present mode.
  const intl::SurfaceFormatKHR surface_format =
      ChooseSurfaceFormat(physical_device->getSurfaceFormatsKHR(*surface));
  const intl::PresentModeKHR present_mode =
      ChoosePresentMode(physical_device->getSurfacePresentModesKHR(*surface));

  // For swapchain images, we don't expect complicated operations, but being
  // rendered to (or resolved to) and then presented to screen.
  // Arbitrary 'attachment_location' would work for image creation.
  const std::vector<ImageUsage> swapchain_image_usages{
      ImageUsage::GetRenderTargetUsage(/*attachment_location=*/0),
      ImageUsage::GetMultisampleResolveTargetUsage(),
      ImageUsage::GetPresentationUsage()};

  // Only graphics queue and presentation queue would access swapchain images.
  const auto& queue_family_indices = physical_device.queue_family_indices();
  const absl::flat_hash_set<uint32_t> queue_family_indices_set{
      queue_family_indices.graphics,
      queue_family_indices.presents.at(window_index),
  };
  const std::vector<uint32_t> unique_queue_family_indices{
      queue_family_indices_set.begin(),
      queue_family_indices_set.end(),
  };

  const auto swapchain_create_info = intl::SwapchainCreateInfoKHR{}
      .setSurface(*surface)
      .setMinImageCount(ChooseMinImageCount(surface))
      .setImageFormat(surface_format.format)
      .setImageColorSpace(surface_format.colorSpace)
      .setImageExtent(image_extent)
      .setImageArrayLayers(CAST_TO_UINT(common::image::kSingleImageLayer))
      .setImageUsage(image::GetImageUsageFlags(swapchain_image_usages))
      .setImageSharingMode(intl::SharingMode::eExclusive)
      .setQueueFamilyIndices(unique_queue_family_indices)
      .setPreTransform(surface.capabilities().currentTransform)
      .setPresentMode(present_mode)
      // Don't care about the color of invisible pixels.
      .setClipped(true);
  swapchain_ = vk_device().createSwapchainKHR(swapchain_create_info,
                                              vk_host_allocator());

  // Fetch swapchain images.
  image_ = std::make_unique<MultiImage>(
      absl::StrFormat("swapchain%d", window_index),
      vk_device().getSwapchainImagesKHR(swapchain_),
      util::ToVec(image_extent), surface_format.format);
}

Swapchain::~Swapchain() {
  context_->DeviceDestroy(swapchain_);
#ifndef NDEBUG
  LOG_INFO << "Swapchain destructed";
#endif  // DEBUG
}

}  // namespace lighter::renderer::vk
