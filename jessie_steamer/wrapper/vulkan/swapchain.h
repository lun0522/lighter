//
//  swapchain.h
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_SWAPCHAIN_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_SWAPCHAIN_H

#include <memory>
#include <vector>

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "third_party/absl/types/optional.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// VkSurfaceKHR interfaces with platform-specific window systems.
// The user should not invoke any other member functions before calling Init().
class Surface {
 public:
  Surface() = default;

  // This class is neither copyable nor movable.
  Surface(const Surface&) = delete;
  Surface& operator=(const Surface&) = delete;

  ~Surface();

  // Overloads.
  const VkSurfaceKHR& operator*() const { return surface_; }

  // Populates the surface information.
  void Init(const BasicContext* context, const VkSurfaceKHR& surface) {
    context_ = context;
    surface_ = surface;
    is_initialized_ = true;
  }

  // Returns the capabilities of the surface.
  VkSurfaceCapabilitiesKHR GetCapabilities() const;

 private:
  // Pointer to context.
  const BasicContext* context_{nullptr};

  // Opaque surface object.
  VkSurfaceKHR surface_;

  // Since 'surface_' is not created when this class is instantiated, we need an
  // extra boolean to determine whether to call vkDestroySurfaceKHR() at
  // destruction.
  bool is_initialized_{false};
};

// VkSwapchainKHR holds a queue of images to present to the screen.
// This is not required for offscreen rendering.
class Swapchain {
 public:
  // Required Vulkan extensions for supporting the swapchain.
  static const std::vector<const char*>& GetRequiredExtensions();

  // If 'multisampling_mode' is not absl::nullopt, we will perform multisampling
  // for swapchain images.
  Swapchain(SharedBasicContext context,
            const Surface& surface, const VkExtent2D& screen_size,
            absl::optional<MultisampleImage::Mode> multisampling_mode);

  // This class is neither copyable nor movable.
  Swapchain(const Swapchain&) = delete;
  Swapchain& operator=(const Swapchain&) = delete;

  ~Swapchain() {
    vkDestroySwapchainKHR(*context_->device(), swapchain_,
                          *context_->allocator());
  }

  // Overloads.
  const VkSwapchainKHR& operator*() const { return swapchain_; }

  // Accessors.
  const VkExtent2D& image_extent() const { return image_extent_; }
  int num_images() const { return swapchain_images_.size(); }
  const Image& image(int index) const { return *swapchain_images_.at(index); }
  const Image& multisample_image() const {
    ASSERT_NON_NULL(multisample_image_, "Multisampling is not enabled");
    return *multisample_image_;
  }

 private:
  // Pointer to context.
  const SharedBasicContext context_;

  // Opaque swapchain object,
  VkSwapchainKHR swapchain_;

  // Wraps images retrieved from the swapchain.
  std::vector<std::unique_ptr<SwapchainImage>> swapchain_images_;

  // This should have value if multisampling is requested. We only need one
  // instance of it since we only render to one frame at any time.
  std::unique_ptr<MultisampleImage> multisample_image_;

  // Extent of each swapchain image.
  VkExtent2D image_extent_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_SWAPCHAIN_H */
