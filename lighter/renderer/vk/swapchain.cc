//
//  swapchain.cc
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/swapchain.h"

#include <algorithm>
#include <format>
#include <limits>
#include <span>
#include <unordered_set>
#include <vector>

#include "lighter/common/image.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/vk/image_util.h"

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
    std::span<const intl::SurfaceFormatKHR> formats) {
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
    std::span<const intl::PresentModeKHR> modes) {
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

// Non-owning vk::Image wrapper representing a single acquired swapchain image.
class SwapchainFrameImage : public Image {
 public:
  SwapchainFrameImage(std::string_view name, const glm::ivec2& extent,
                      intl::Format format, intl::Image image,
                      intl::ImageView image_view)
      : Image{name,   LayerType::kSingle,           extent, /*mip_levels=*/1,
              format, intl::SampleCountFlagBits::e1},
        image_{image},
        image_view_{image_view} {}

  intl::ImageView image_view() const override { return image_view_; }
  intl::Image vk_image() const override { return image_; }

 private:
  intl::Image image_;
  intl::ImageView image_view_;
};

}  // namespace

Swapchain::Swapchain(const SharedContext& context, int window_index,
                     const common::Window& window)
    : WithSharedContext{context}, window_index_{window_index} {
  // Refresh surface capabilities so resize sees current extents.
  const PhysicalDevice& physical_device = context_->physical_device();
  Surface& surface = context_->mutable_surface(window_index);
  surface.SetCapabilities(physical_device->getSurfaceCapabilitiesKHR(*surface));
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
  const std::unordered_set<uint32_t> queue_family_indices_set{
      queue_family_indices.graphics,
      queue_family_indices.presents.at(window_index),
  };
  const std::vector<uint32_t> unique_queue_family_indices{
      queue_family_indices_set.begin(),
      queue_family_indices_set.end(),
  };

  const auto swapchain_create_info =
      intl::SwapchainCreateInfoKHR{}
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

  format_ = surface_format.format;
  extent_ = image_extent;
  images_ = vk_device().getSwapchainImagesKHR(swapchain_);
  image_views_.reserve(images_.size());
  frame_images_.reserve(images_.size());
  for (size_t i = 0; i < images_.size(); ++i) {
    const intl::ImageView view = image::CreateImageView(
        *context_, images_[i], intl::ImageViewType::e2D, format_,
        intl::ImageAspectFlagBits::eColor, /*mip_levels=*/1,
        /*layer_count=*/1);
    image_views_.push_back(view);
    frame_images_.push_back(std::make_unique<SwapchainFrameImage>(
        std::format("swapchain{}_image{}", window_index, i),
        util::ToVec(extent_), format_, images_[i], view));
  }

  render_finished_semaphores_.resize(images_.size());
  for (intl::Semaphore& semaphore : render_finished_semaphores_) {
    semaphore = vk_device().createSemaphore(intl::SemaphoreCreateInfo{},
                                            vk_host_allocator());
  }
}

Swapchain::~Swapchain() {
  for (intl::Semaphore semaphore : render_finished_semaphores_) {
    context_->DeviceDestroy(semaphore);
  }
  frame_images_.clear();
  if (!image_views_.empty()) {
    // Destroy image views before destroying the backing swapchain.
    std::vector<intl::ImageView> expired_views = std::move(image_views_);
    context_->AddReleaseExpiredResourceOp(
        [views = std::move(expired_views)](const Context& context) {
          for (intl::ImageView view : views) {
            context.device()->destroyImageView(view, *context.host_allocator());
          }
        });
  }
  context_->DeviceDestroy(swapchain_);
#ifndef NDEBUG
  LOG_INFO << "Swapchain destructed";
#endif  // DEBUG
}

intl::Result Swapchain::AcquireNextImage(intl::Semaphore signal_semaphore,
                                         uint32_t* out_image_index) const {
  return vk_device().acquireNextImageKHR(
      swapchain_, std::numeric_limits<uint64_t>::max(), signal_semaphore,
      intl::Fence{}, out_image_index);
}

intl::Result Swapchain::Present(uint32_t image_index,
                                intl::Semaphore wait_semaphore) const {
  const auto present_info = intl::PresentInfoKHR{}
                                .setWaitSemaphores(wait_semaphore)
                                .setSwapchains(swapchain_)
                                .setImageIndices(image_index);
  return context_->queues()
      .present_queue(window_index_)
      .presentKHR(&present_info);
}

}  // namespace lighter::renderer::vk
