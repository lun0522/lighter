//
//  image.h
//
//  Created by Pujun Lun on 3/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_H

#include <array>

#include "absl/types/variant.h"
#include "jessie_steamer/common/ref_count.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/util.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

/** VkImage represents multidimensional data in the swapchain. They can be
 *    color/depth/stencil attachments, textures, etc. The exact purpose
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
 *    Use image coordinates or normalized coordinates
 */
class Image {
 public:
  virtual ~Image() {
    vkDestroyImageView(*context_->device(), image_view_, context_->allocator());
  }

  const VkImageView& image_view() const { return image_view_; }
  const VkExtent2D& extent() const { return extent_; }
  VkFormat format() const { return format_; }

 protected:
  Image(SharedBasicContext context, const VkExtent2D& extent, VkFormat format)
      : context_{std::move(context)}, extent_{extent}, format_{format} {}

  SharedBasicContext context_;
  VkImageView image_view_;
  const VkExtent2D extent_;
  const VkFormat format_;
};

class SamplableImage {
 public:
  virtual ~SamplableImage() = default;
  virtual VkDescriptorImageInfo descriptor_info() const = 0;
};

class TextureImage : public Image {
 public:
  TextureImage(SharedBasicContext context,
               bool generate_mipmaps,
               const TextureBuffer::Info& info);

  // This class is neither copyable nor movable.
  TextureImage(const TextureImage&) = delete;
  TextureImage& operator=(const TextureImage&) = delete;

  ~TextureImage() override {
    vkDestroySampler(*context_->device(), sampler_, context_->allocator());
  }

  VkDescriptorImageInfo descriptor_info() const;

 private:
  TextureBuffer buffer_;
  VkSampler sampler_;
};

class SharedTexture : public SamplableImage {
 public:
  // Textures will be put in a unified resource pool. For single images, its
  // file path will be used as identifier; for cubemaps, its directory will be
  // used as identifier.
  using SingleTexPath = std::string;
  struct CubemapPath {
    std::string directory;
    // PosX, NegX, PosY, NegY, PosZ, NegZ.
    std::array<std::string, kCubemapImageCount> files;
  };
  using SourcePath = absl::variant<SingleTexPath, CubemapPath>;

  SharedTexture(SharedBasicContext context, const SourcePath& source_path)
      : texture_{GetTexture(std::move(context), source_path)} {}

  // This class is only movable.
  SharedTexture(SharedTexture&&) = default;
  SharedTexture& operator=(SharedTexture&&) = default;

  VkDescriptorImageInfo descriptor_info() const override {
    return texture_->descriptor_info();
  }

 private:
  using RefCountedTexture = common::RefCountedObject<TextureImage>;
  static RefCountedTexture GetTexture(SharedBasicContext context,
                                      const SourcePath& source_path);

  RefCountedTexture texture_;
};

class OffscreenImage : public Image {
 public:
  OffscreenImage(SharedBasicContext context,
                 int channel, const VkExtent2D& extent);

  // This class is neither copyable nor movable.
  OffscreenImage(const OffscreenImage&) = delete;
  OffscreenImage& operator=(const OffscreenImage&) = delete;

  ~OffscreenImage() override {
    vkDestroySampler(*context_->device(), sampler_, context_->allocator());
  }

  VkDescriptorImageInfo descriptor_info() const;

 private:
  OffscreenBuffer buffer_;
  VkSampler sampler_;
};

using OffscreenImagePtr = const OffscreenImage*;
class UnownedOffscreenTexture : public SamplableImage {
 public:
  explicit UnownedOffscreenTexture(OffscreenImagePtr texture)
      : texture_{texture} {}

  VkDescriptorImageInfo descriptor_info() const override {
    return texture_->descriptor_info();
  }

 private:
  OffscreenImagePtr texture_;
};

class DepthStencilImage : public Image {
 public:
  static VkFormat GetDepthStencilImageFormat(const SharedBasicContext& context);

  DepthStencilImage(const SharedBasicContext& context,
                    const VkExtent2D& extent);

  // This class is neither copyable nor movable.
  DepthStencilImage(const DepthStencilImage&) = delete;
  DepthStencilImage& operator=(const DepthStencilImage&) = delete;

 private:
  DepthStencilBuffer buffer_;
};

class SwapchainImage : public Image {
 public:
  SwapchainImage(SharedBasicContext context,
                 const VkImage& image,
                 const VkExtent2D& extent, VkFormat format);

  // This class is neither copyable nor movable.
  SwapchainImage(const SwapchainImage&) = delete;
  SwapchainImage& operator=(const SwapchainImage&) = delete;
};

class MultisampleImage : public Image {
 public:
  enum class Mode { kBestEffect, kEfficient };

  static std::unique_ptr<MultisampleImage> CreateColorMultisampleImage(
      SharedBasicContext context,
      const Image& target_image, Mode mode);

  static std::unique_ptr<MultisampleImage> CreateDepthStencilMultisampleImage(
      SharedBasicContext context,
      const VkExtent2D& extent, Mode mode);

  MultisampleImage(SharedBasicContext context,
                   const VkExtent2D& extent, VkFormat format,
                   Mode mode, MultisampleBuffer::Type type);

  // This class is neither copyable nor movable.
  MultisampleImage(const MultisampleImage&) = delete;
  MultisampleImage& operator=(const MultisampleImage&) = delete;

  VkSampleCountFlagBits sample_count() const { return sample_count_; }

 private:
  const VkSampleCountFlagBits sample_count_;
  MultisampleBuffer buffer_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_H */
