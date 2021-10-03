//
//  image.h
//
//  Created by Pujun Lun on 3/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_WRAPPER_IMAGE_H
#define LIGHTER_RENDERER_VULKAN_WRAPPER_IMAGE_H

#include <array>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "lighter/common/file.h"
#include "lighter/common/image.h"
#include "lighter/common/ref_count.h"
#include "lighter/common/util.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/buffer.h"
#include "lighter/renderer/vulkan/wrapper/util.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {

using ir::ImageUsage;

// This class creates a chunk of memory that is visible to both host and device,
// used for transferring image data from the host to some memory that is only
// visible to the device. When construction is done, the data is already sent
// from the host to the underlying buffer object.
class ImageStagingBuffer : public StagingBuffer {
 public:
  // Inherits constructor.
  using StagingBuffer::StagingBuffer;

  // This class is neither copyable nor movable.
  ImageStagingBuffer(const ImageStagingBuffer&) = delete;
  ImageStagingBuffer& operator=(const ImageStagingBuffer&) = delete;

  // Copies image data from this buffer to the targeted image, assuming the
  // layout of 'target' is TRANSFER_DST_OPTIMAL.
  void CopyToImage(const VkImage& target, const VkExtent3D& image_extent,
                   uint32_t image_layer_count) const;
};

// This is the base class of buffers storing images. The user should use it
// through derived classes. Since all buffers of this kind need VkImage,
// which configures how do we use the device memory to store multidimensional
// data, it will be held and destroyed by this base class, and initialized by
// derived classes.
class ImageBuffer : public Buffer {
 public:
  // This class is neither copyable nor movable.
  ImageBuffer(const ImageBuffer&) = delete;
  ImageBuffer& operator=(const ImageBuffer&) = delete;

  ~ImageBuffer() override {
    vkDestroyImage(*context_->device(), image_, *context_->allocator());
  }

  // Accessors.
  const VkImage& image() const { return image_; }

 protected:
  // Inherits constructor.
  using Buffer::Buffer;

  // Modifiers.
  void set_image(const VkImage& image) { image_ = image; }

 private:
  // Opaque image object.
  VkImage image_;
};

// This is the base class of all image classes. The user should use it through
// derived classes. Since all images need VkImageView, which configures how do
// we interpret the multidimensional data stored with VkImage, it will be held
// and destroyed by this base class, and initialized by derived classes.
class Image {
 public:
  // This class is neither copyable nor movable.
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  virtual ~Image() {
    vkDestroyImageView(*context_->device(), image_view_,
                       *context_->allocator());
  }

  // Returns descriptor types used for updating descriptor sets.
  static VkDescriptorType GetDescriptorTypeForSampling() {
    return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  }
  static VkDescriptorType GetDescriptorTypeForLinearAccess() {
    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  }

  // Returns the image usage right after it is constructed. The user is
  // responsible for tracking usage changes afterwards.
  virtual ImageUsage GetInitialUsage() const { return ImageUsage{}; }

  // Overloads.
  const VkImage& operator*() const { return image(); }

  // Accessors.
  virtual const VkImage& image() const = 0;
  const VkImageView& image_view() const { return image_view_; }
  const VkExtent2D& extent() const { return extent_; }
  VkFormat format() const { return format_; }
  virtual VkSampleCountFlagBits sample_count() const {
    return VK_SAMPLE_COUNT_1_BIT;
  }

 protected:
  Image(SharedBasicContext context, const VkExtent2D& extent, VkFormat format)
      : context_{std::move(FATAL_IF_NULL(context))},
        extent_{extent}, format_{format} {}

  // Modifiers.
  void set_image_view(const VkImageView& image_view) {
    image_view_ = image_view;
  }

  // Pointer to context.
  const SharedBasicContext context_;

  // Image extent.
  const VkExtent2D extent_;

  // Image format.
  const VkFormat format_;

 private:
  // Opaque image view object.
  VkImageView image_view_;
};

// VkSampler configures how do we sample from an image resource on the device.
class ImageSampler {
 public:
  // Configures sampling mode.
  struct Config {
    explicit Config(
        VkFilter filter = VK_FILTER_LINEAR,
        VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT)
        : filter{filter}, address_mode{address_mode} {}

    VkFilter filter;
    VkSamplerAddressMode address_mode;
  };

  ImageSampler(SharedBasicContext context,
               int mip_levels, const Config& config);

  // This class is neither copyable nor movable.
  ImageSampler(const ImageSampler&) = delete;
  ImageSampler& operator=(const ImageSampler&) = delete;

  ~ImageSampler() {
    vkDestroySampler(*context_->device(), sampler_, *context_->allocator());
  }

  // Overloads.
  const VkSampler& operator*() const { return sampler_; }

 private:
  // Pointer to context.
  const SharedBasicContext context_;

  // Opaque image sampler object.
  VkSampler sampler_;
};

// Interface of images that can be sampled.
class SamplableImage {
 public:
  virtual ~SamplableImage() = default;

  // Returns an instance of VkDescriptorImageInfo with which we can update
  // descriptor sets.
  virtual VkDescriptorImageInfo GetDescriptorInfo(
      VkImageLayout layout) const = 0;
  VkDescriptorImageInfo GetDescriptorInfoForSampling() const {
    return GetDescriptorInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
  VkDescriptorImageInfo GetDescriptorInfoForLinearAccess() const {
    return GetDescriptorInfo(VK_IMAGE_LAYOUT_GENERAL);
  }
};

// This class copies a texture image on the host to device via a staging buffer,
// and generates mipmaps if requested.
// If the image is loaded from a file, the user should not directly instantiate
// this class, but use SharedTexture which avoids loading the same file twice.
class TextureImage : public Image, public SamplableImage {
 public:
  // Description of the image data. The size of 'datas' can only be either 1
  // or 6 (for cubemaps), otherwise, the constructor will throw an exception.
  struct Info{
    // Returns the extent of image.
    VkExtent2D GetExtent2D() const { return {width, height}; }
    VkExtent3D GetExtent3D() const { return {width, height, /*depth=*/1}; }

    // Returns an instance of CopyInfos that can be used for copying image data
    // from the host to device memory.
    Buffer::CopyInfos GetCopyInfos() const;

    std::vector<const void*> data_ptrs;
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t channel;
    absl::Span<const ImageUsage> usages;
  };

  TextureImage(SharedBasicContext context,
               bool generate_mipmaps,
               const ImageSampler::Config& sampler_config,
               const Info& info);

  TextureImage(const SharedBasicContext& context,
               bool generate_mipmaps,
               const common::Image& image,
               absl::Span<const ImageUsage> usages,
               const ImageSampler::Config& sampler_config);

  // This class is neither copyable nor movable.
  TextureImage(const TextureImage&) = delete;
  TextureImage& operator=(const TextureImage&) = delete;

  // Overrides.
  const VkImage& image() const override { return buffer_.image(); }
  VkDescriptorImageInfo GetDescriptorInfo(VkImageLayout layout) const override {
    return {*sampler_, image_view(), layout};
  }
  ImageUsage GetInitialUsage() const override {
    return ImageUsage::GetSampledInFragmentShaderUsage();
  }

 private:
  // Texture image buffer on the device.
  class TextureBuffer : public ImageBuffer {
   public:
    TextureBuffer(SharedBasicContext context,
                  bool generate_mipmaps, const Info& info);

    // This class is neither copyable nor movable.
    TextureBuffer(const TextureBuffer&) = delete;
    TextureBuffer& operator=(const TextureBuffer&) = delete;

    // Accessors.
    int mip_levels() const { return mip_levels_; }

   private:
    // Mipmapping levels.
    int mip_levels_ = kSingleMipLevel;
  };

  // Image buffer.
  const TextureBuffer buffer_;

  // Image sampler.
  const ImageSampler sampler_;
};

// This class references to a texture image on the device, which is reference
// counted. The texture image in the internal resource pool is identified by a
// string. For single images, the file path will be used as identifier, while
// for cubemaps, the directory will be used as identifier. The user may create
// multiple instances of this class with the same path, and they will reference
// to the same resource in the pool.
// Mipmaps will be generated for single images, not for cubemaps.
class SharedTexture : public SamplableImage {
 public:
  // The user should either provide one file path for a single image, or a
  // directory with 6 relative paths for a cubemap.
  using SingleTexPath = std::string;
  struct CubemapPath {
    std::string directory;
    // PosX, NegX, PosY, NegY, PosZ, NegZ.
    std::array<std::string, common::image::kCubemapImageLayer> files;
  };
  using SourcePath = std::variant<SingleTexPath, CubemapPath>;

  SharedTexture(const SharedBasicContext& context,
                const SourcePath& source_path,
                absl::Span<const ImageUsage> usages,
                const ImageSampler::Config& sampler_config)
      : texture_{GetTexture(context, source_path, usages, sampler_config)} {}

  // This class is only movable.
  SharedTexture(SharedTexture&&) noexcept = default;
  SharedTexture& operator=(SharedTexture&&) noexcept = default;

  // Overrides.
  VkDescriptorImageInfo GetDescriptorInfo(VkImageLayout layout) const override {
    return texture_->GetDescriptorInfo(layout);
  }

  // Overloads.
  const Image* operator->() const { return texture_.operator->(); }

 private:
  // Reference counted texture.
  using RefCountedTexture = common::RefCountedObject<TextureImage>;

  // Returns a reference to a reference counted texture image. If this image has
  // no other holder, it will be loaded from the file. Otherwise, this returns
  // a reference to an existing resource on the device.
  static RefCountedTexture GetTexture(
      const SharedBasicContext& context,
      const SourcePath& source_path,
      absl::Span<const ImageUsage> usages,
      const ImageSampler::Config& sampler_config);

  // Reference counted texture image.
  RefCountedTexture texture_;
};

// This class creates an image that can be used for offscreen rendering and
// compute shaders. No data transfer is required at construction.
class OffscreenImage : public Image, public SamplableImage {
 public:
  OffscreenImage(SharedBasicContext context,
                 const VkExtent2D& extent, VkFormat format,
                 absl::Span<const ImageUsage> usages,
                 const ImageSampler::Config& sampler_config);

  // Only 1 or 4 channels are supported.
  OffscreenImage(const SharedBasicContext& context,
                 const VkExtent2D& extent, int channel,
                 absl::Span<const ImageUsage> usages,
                 const ImageSampler::Config& sampler_config,
                 bool use_high_precision);

  // This class is neither copyable nor movable.
  OffscreenImage(const OffscreenImage&) = delete;
  OffscreenImage& operator=(const OffscreenImage&) = delete;

  // Overrides.
  const VkImage& image() const override { return buffer_.image(); }
  VkDescriptorImageInfo GetDescriptorInfo(VkImageLayout layout) const override {
    return {*sampler_, image_view(), layout};
  }

 private:
  // Offscreen image buffer on the device.
  class OffscreenBuffer : public ImageBuffer {
   public:
    OffscreenBuffer(SharedBasicContext context,
                    const VkExtent2D& extent, VkFormat format,
                    absl::Span<const ImageUsage> usages);

    // This class is neither copyable nor movable.
    OffscreenBuffer(const OffscreenBuffer&) = delete;
    OffscreenBuffer& operator=(const OffscreenBuffer&) = delete;
  };

  // Image buffer.
  const OffscreenBuffer buffer_;

  // Image sampler.
  const ImageSampler sampler_;
};

using OffscreenImagePtr = const OffscreenImage*;

// This class internally holds a reference to an image for offscreen rendering.
// Note that this is an unowned relationship, hence the user is responsible for
// keeping the existence of the resource before done using this class.
class UnownedOffscreenTexture : public SamplableImage {
 public:
  explicit UnownedOffscreenTexture(OffscreenImagePtr texture)
      : texture_{FATAL_IF_NULL(texture)} {}

  // This class provides copy constructor and move constructor.
  UnownedOffscreenTexture(UnownedOffscreenTexture&&) noexcept = default;
  UnownedOffscreenTexture(const UnownedOffscreenTexture&) = default;

  // Overrides.
  VkDescriptorImageInfo GetDescriptorInfo(VkImageLayout layout) const override {
    return texture_->GetDescriptorInfo(layout);
  }

  // Overloads.
  const Image* operator->() const { return texture_; }

 private:
  // Pointer to image.
  const OffscreenImagePtr texture_;
};

// This class creates an image that can be used as depth stencil attachment.
// No data transfer is required at construction.
class DepthStencilImage : public Image {
 public:
  DepthStencilImage(const SharedBasicContext& context,
                    const VkExtent2D& extent);

  // This class is neither copyable nor movable.
  DepthStencilImage(const DepthStencilImage&) = delete;
  DepthStencilImage& operator=(const DepthStencilImage&) = delete;

  // Overrides.
  const VkImage& image() const override { return buffer_.image(); }

 private:
  // Depth stencil image buffer on the device.
  class DepthStencilBuffer : public ImageBuffer {
   public:
    DepthStencilBuffer(SharedBasicContext context,
                       const VkExtent2D& extent, VkFormat format);

    // This class is neither copyable nor movable.
    DepthStencilBuffer(const DepthStencilBuffer&) = delete;
    DepthStencilBuffer& operator=(const DepthStencilBuffer&) = delete;
  };

  // Image buffer.
  const DepthStencilBuffer buffer_;
};

// This class references to an existing swapchain image. The user is responsible
// for keeping the existence of the swapchain before done using this class.
class SwapchainImage : public Image {
 public:
  SwapchainImage(SharedBasicContext context,
                 const VkImage& image,
                 const VkExtent2D& extent, VkFormat format);

  // This class is neither copyable nor movable.
  SwapchainImage(const SwapchainImage&) = delete;
  SwapchainImage& operator=(const SwapchainImage&) = delete;

  // Overrides.
  const VkImage& image() const override { return image_; }

 private:
  // Swapchain image.
  VkImage image_;
};

// This class creates an image for multisampling. No data transfer is required
// at construction.
class MultisampleImage : public Image {
 public:
  // Multisampling modes that determine the quality of rendering.
  enum class Mode {
    // Use "just OK" number of sampling points. This is set to 4 internally.
    kEfficient,
    // Use the largest number of sampling points that can be supported by the
    // physical device. We will pay price in performance for better effects.
    kBestEffect,
  };

  // Returns a multisample image for a regular color image 'target_image'.
  static std::unique_ptr<Image> CreateColorMultisampleImage(
      SharedBasicContext context,
      const Image& target_image, Mode mode);

  // Returns a multisample image that can be used as depth stencil attachment.
  // Note that we don't need to resolve this image to another regular image.
  static std::unique_ptr<Image> CreateDepthStencilMultisampleImage(
      SharedBasicContext context,
      const VkExtent2D& extent, Mode mode);

  // Convenience function for creating a depth stencil image. Whether the image
  // is a multisample image depends on whether 'mode' has value.
  // Since we don't need to resolve multisampling depth stencil images, we can
  // directly use whatever image returned by this function.
  static std::unique_ptr<Image> CreateDepthStencilImage(
      SharedBasicContext context,
      const VkExtent2D& extent, std::optional<Mode> mode);

  // This class is neither copyable nor movable.
  MultisampleImage(const MultisampleImage&) = delete;
  MultisampleImage& operator=(const MultisampleImage&) = delete;

  // Overrides.
  const VkImage& image() const override { return buffer_.image(); }

  // Accessors.
  VkSampleCountFlagBits sample_count() const override { return sample_count_; }

 private:
  // Multisample image buffer on the device.
  class MultisampleBuffer : public ImageBuffer {
   public:
    // The multisample image resolves to either color attachments or depth
    // stencil attachments.
    enum class Type { kColor, kDepthStencil };

    MultisampleBuffer(SharedBasicContext context,
                      Type type, const VkExtent2D& extent, VkFormat format,
                      VkSampleCountFlagBits sample_count);

    // This class is neither copyable nor movable.
    MultisampleBuffer(const MultisampleBuffer&) = delete;
    MultisampleBuffer& operator=(const MultisampleBuffer&) = delete;
  };

  MultisampleImage(SharedBasicContext context,
                   const VkExtent2D& extent, VkFormat format,
                   Mode mode, MultisampleBuffer::Type type);

  // Returns the number of samples per pixel chosen according to 'mode' and
  // physical device limits.
  VkSampleCountFlagBits ChooseSampleCount(Mode mode);

  // Number of samples per pixel.
  const VkSampleCountFlagBits sample_count_;

  // Image buffer.
  const MultisampleBuffer buffer_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_WRAPPER_IMAGE_H */
