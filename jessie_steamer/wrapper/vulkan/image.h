//
//  image.h
//
//  Created by Pujun Lun on 3/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_H

#include <array>
#include <memory>
#include <vector>

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class Context;

/** VkImage represents multidimensional data in the swapchain. They can be
 *    color/depth/stencil attachements, textures, etc. The exact purpose
 *    is not specified until we create an image view. For texture images, we use
 *    staging buffers to transfer data to the actual storage. During this
 *    process, the image layout wil change from UNDEFINED (since it may have
 *    very different layouts when it is on host, which makes it unusable for
 *    device) to TRANSFER_DST_OPTIMAL (so that we can transfer data from the
 *    staging buffer to it), and eventually to SHADER_READ_ONLY_OPTIMAL (optimal
 *    for device access).
 *
 *  Initialization:
 *    VkDevice
 *    VkSwapchainKHR
 *
 *------------------------------------------------------------------------------
 *
 *  VkImageView determines how to access and what part of images to access.
 *    We might convert the image format on the fly with it.
 *
 *  Initialization:
 *    VkDevice
 *    Image referenced by it
 *    View type (1D, 2D, 3D, cube, etc.)
 *    Format of the image
 *    Whether and how to remap RGBA channels
 *    Purpose of the image (color, depth, stencil, etc)
 *    Set of mipmap levels and array layers to be accessible
 *
 *------------------------------------------------------------------------------
 *
 *  VkSampler configures how do we sample and filter images.
 *
 *  Initialization:
 *    Minification/magnification filter mode (either nearest or linear)
 *    Mipmap mode (either nearest or linear)
 *    Address mode for u/v/w ([mirror] repeat, [mirror] clamp to edge/border)
 *    Mipmap setting (bias/min/max/compare op)
 *    Anisotropy filter setting (enable or not/max amount of samples)
 *    Border color
 *    Use image coordinates or normalized coordianates
 */

class SwapChainImage {
 public:
  SwapChainImage() = default;
  void Init(std::shared_ptr<Context> context,
            const VkImage& image,
            VkFormat format);
  ~SwapChainImage();

  // This class is only movable
  SwapChainImage(SwapChainImage&&) = default;
  SwapChainImage& operator=(SwapChainImage&&) = default;

  const VkImageView& image_view() const { return image_view_; }

 private:
  std::shared_ptr<Context> context_;
  VkImageView image_view_;
};

class TextureImage {
 public:
  TextureImage() = default;
  // paths.size() should be either 1 or 6 (cubemap)
  void Init(std::shared_ptr<Context> context,
            const std::vector<std::string>& paths);
  void Init(std::shared_ptr<Context> context,
            const std::vector<common::util::Image>& images);
  ~TextureImage();

  // This class is only movable
  TextureImage(TextureImage&& rhs) noexcept {
    context_ = std::move(rhs.context_);
    buffer_ = std::move(rhs.buffer_);
    image_view_ = std::move(rhs.image_view_);
    sampler_ = std::move(rhs.sampler_);
    rhs.context_ = nullptr;
  }

  TextureImage& operator=(TextureImage&& rhs) noexcept {
    context_ = std::move(rhs.context_);
    buffer_ = std::move(rhs.buffer_);
    image_view_ = std::move(rhs.image_view_);
    sampler_ = std::move(rhs.sampler_);
    rhs.context_ = nullptr;
    return *this;
  }

  VkDescriptorImageInfo descriptor_info() const;

 private:
  std::shared_ptr<Context> context_;
  TextureBuffer buffer_;
  VkImageView image_view_;
  VkSampler sampler_;
};

class DepthStencilImage {
 public:
  DepthStencilImage() = default;
  void Init(std::shared_ptr<Context> context,
            VkExtent2D extent);
  void Cleanup();
  ~DepthStencilImage() { Cleanup(); }

  // This class is only movable
  DepthStencilImage(DepthStencilImage&&) = default;
  DepthStencilImage& operator=(DepthStencilImage&&) = default;

  VkFormat format()               const { return buffer_.format(); }
  const VkImageView& image_view() const { return image_view_; }

 private:
  std::shared_ptr<Context> context_;
  DepthStencilBuffer buffer_;
  VkImageView image_view_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_H */
