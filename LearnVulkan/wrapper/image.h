//
//  image.h
//
//  Created by Pujun Lun on 3/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef WRAPPER_VULKAN_IMAGE_H
#define WRAPPER_VULKAN_IMAGE_H

#include <memory>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "buffer.h"

namespace wrapper {
namespace vulkan {

class Context;

/** VkImage represents multidimensional data in the swap chain. They can be
 *      color/depth/stencil attachements, textures, etc. The exact purpose
 *      is not specified until we create an image view.
 *
 *  Initialization:
 *      VkDevice
 *      VkSwapchainKHR
 *
 *------------------------------------------------------------------------------
 *
 *  VkImageView determines how to access and what part of images to access.
 *      We might convert the image format on the fly with it.
 *
 *  Initialization:
 *      VkDevice
 *      Image referenced by it
 *      View type (1D, 2D, 3D, cube, etc.)
 *      Format of the image
 *      Whether and how to remap RGBA channels
 *      Purpose of the image (color, depth, stencil, etc)
 *      Set of mipmap levels and array layers to be accessible
 */

class SwapChainImage {
 public:
  SwapChainImage() = default;
  void Init(std::shared_ptr<Context> context,
            const VkImage& image,
            VkFormat format);
  ~SwapChainImage();

  // This class is neither copyable nor movable
  SwapChainImage(const SwapChainImage&) = delete;
  SwapChainImage& operator=(const SwapChainImage&) = delete;

  const VkImageView& image_view() const { return image_view_; }

 private:
  std::shared_ptr<Context> context_;
  VkImageView image_view_;
};

class TextureImage {
 public:
  TextureImage() = default;
  void Init(std::shared_ptr<Context> context,
            const std::string& path);
  ~TextureImage();

  // This class is neither copyable nor movable
  TextureImage(const TextureImage&) = delete;
  TextureImage& operator=(const TextureImage&) = delete;

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

  // This class is neither copyable nor movable
  DepthStencilImage(const DepthStencilImage&) = delete;
  DepthStencilImage& operator=(const DepthStencilImage&) = delete;

  VkFormat format()               const { return buffer_.format(); }
  const VkImageView& image_view() const { return image_view_; }

 private:
  std::shared_ptr<Context> context_;
  DepthStencilBuffer buffer_;
  VkImageView image_view_;
};

} /* namespace vulkan */
} /* namespace wrapper */

#endif /* WRAPPER_VULKAN_IMAGE_H */
