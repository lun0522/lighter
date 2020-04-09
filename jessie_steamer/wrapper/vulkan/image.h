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

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/common/ref_count.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/util.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/variant.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

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
  void SetImage(const VkImage& image) { image_ = image; }

 private:
  // Opaque image object.
  VkImage image_;
};

// This class copies an image on the host to device via the staging buffer,
// and generates mipmaps if requested.
class TextureBuffer : public ImageBuffer {
 public:
  // Description of the image data. The size of 'datas' can only be either 1
  // or 6 (for cubemaps), otherwise, the constructor will throw an exception.
  struct Info{
    // Returns the extent of image.
    VkExtent2D GetExtent2D() const { return {width, height}; }
    VkExtent3D GetExtent3D() const { return {width, height, /*depth=*/1}; }

    // Returns an instance of CopyInfos that can be used for copying image data
    // from the host to device memory.
    CopyInfos GetCopyInfos() const;

    std::vector<const void*> datas;
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t channel;
  };

  TextureBuffer(SharedBasicContext context,
                bool generate_mipmaps, const Info& info);

  // This class is neither copyable nor movable.
  TextureBuffer(const TextureBuffer&) = delete;
  TextureBuffer& operator=(const TextureBuffer&) = delete;

  // Accessors.
  uint32_t mip_levels() const { return mip_levels_; }

 private:
  // Level of mipmaps.
  uint32_t mip_levels_;
};

// This class creates an image buffer that can be used for offscreen rendering
// and compute shaders. No data transfer is required at construction.
class OffscreenBuffer : public ImageBuffer {
 public:
  // This buffer is used as either rendering target or compute shader output.
  enum class DataSource { kRender, kCompute };

  OffscreenBuffer(SharedBasicContext context, DataSource data_source,
                  const VkExtent2D& extent, VkFormat format);

  // This class is neither copyable nor movable.
  OffscreenBuffer(const OffscreenBuffer&) = delete;
  OffscreenBuffer& operator=(const OffscreenBuffer&) = delete;
};

// This class creates an image buffer that can be used as depth stencil image
// buffer. No data transfer is required at construction.
class DepthStencilBuffer : public ImageBuffer {
 public:
  DepthStencilBuffer(SharedBasicContext context,
                     const VkExtent2D& extent, VkFormat format);

  // This class is neither copyable nor movable.
  DepthStencilBuffer(const DepthStencilBuffer&) = delete;
  DepthStencilBuffer& operator=(const DepthStencilBuffer&) = delete;
};

// This class creates an image buffer for multisampling. No data transfer is
// required at construction.
class MultisampleBuffer : public ImageBuffer {
 public:
  enum class Type { kColor, kDepthStencil };

  MultisampleBuffer(SharedBasicContext context,
                    Type type, const VkExtent2D& extent, VkFormat format,
                    VkSampleCountFlagBits sample_count);

  // This class is neither copyable nor movable.
  MultisampleBuffer(const MultisampleBuffer&) = delete;
  MultisampleBuffer& operator=(const MultisampleBuffer&) = delete;
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

  // Accessors.
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
  void SetImageView(const VkImageView& image_view) { image_view_ = image_view; }

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

// Interface of images that can be sampled.
class SamplableImage {
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

  virtual ~SamplableImage() = default;

  // Returns an instance of VkDescriptorImageInfo with which we can update
  // descriptor sets.
  virtual VkDescriptorImageInfo GetDescriptorInfo() const = 0;
};

// This class creates a texture image resource on the device, and generates
// mipmaps if requested.
// If the image is loaded from a file, the user should not directly instantiate
// this class, but use SharedTexture which avoids loading the same file twice.
class TextureImage : public Image, public SamplableImage {
 public:
  TextureImage(SharedBasicContext context,
               bool generate_mipmaps,
               const SamplableImage::Config& sampler_config,
               const TextureBuffer::Info& info);

  TextureImage(SharedBasicContext context,
               bool generate_mipmaps,
               const common::Image& image,
               const SamplableImage::Config& sampler_config);

  // This class is neither copyable nor movable.
  TextureImage(const TextureImage&) = delete;
  TextureImage& operator=(const TextureImage&) = delete;

  ~TextureImage() override {
    vkDestroySampler(*context_->device(), sampler_, *context_->allocator());
  }

  // Overrides.
  VkDescriptorImageInfo GetDescriptorInfo() const override;

  // TODO: Remove.
  const VkImage& image() const { return buffer_.image(); }

 private:
  // Image buffer.
  TextureBuffer buffer_;

  // Image sampler.
  VkSampler sampler_;
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
    std::array<std::string, common::kCubemapImageCount> files;
  };
  using SourcePath = absl::variant<SingleTexPath, CubemapPath>;

  SharedTexture(SharedBasicContext context, const SourcePath& source_path,
                const SamplableImage::Config& sampler_config)
      : texture_{GetTexture(std::move(context), source_path, sampler_config)} {}

  // This class is only movable.
  SharedTexture(SharedTexture&&) = default;
  SharedTexture& operator=(SharedTexture&&) = default;

  // Overrides.
  VkDescriptorImageInfo GetDescriptorInfo() const override {
    return texture_->GetDescriptorInfo();
  }

  // Overloads.
  const Image* operator->() const { return texture_.operator->(); }

 private:
  // Reference counted textures.
  using RefCountedTexture = common::RefCountedObject<TextureImage>;

  // Returns a reference to a reference counted texture image. If this image has
  // no other holder, it will be loaded from the file. Otherwise, this returns
  // a reference to an existing resource on the device.
  static RefCountedTexture GetTexture(
      SharedBasicContext context, const SourcePath& source_path,
      const SamplableImage::Config& sampler_config);

  // Reference counted texture image.
  RefCountedTexture texture_;
};

// This class creates an image that can be used for offscreen rendering and
// compute shaders.
class OffscreenImage : public Image, public SamplableImage {
 public:
  using DataSource = OffscreenBuffer::DataSource;

  OffscreenImage(SharedBasicContext context,
                 DataSource data_source, int channel, const VkExtent2D& extent,
                 const SamplableImage::Config& sampler_config);

  // This class is neither copyable nor movable.
  OffscreenImage(const OffscreenImage&) = delete;
  OffscreenImage& operator=(const OffscreenImage&) = delete;

  ~OffscreenImage() override {
    vkDestroySampler(*context_->device(), sampler_, *context_->allocator());
  }

  // Overrides.
  VkDescriptorImageInfo GetDescriptorInfo() const override;

  // TODO: Remove.
  const VkImage& image() const { return buffer_.image(); }

 private:
  // Image buffer.
  OffscreenBuffer buffer_;

  // Image sampler.
  VkSampler sampler_;
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
  UnownedOffscreenTexture(UnownedOffscreenTexture&&) = default;
  UnownedOffscreenTexture(UnownedOffscreenTexture&) = default;

  // Overrides.
  VkDescriptorImageInfo GetDescriptorInfo() const override {
    return texture_->GetDescriptorInfo();
  }

  // Overloads.
  const Image* operator->() const { return texture_; }

 private:
  // Pointer to image.
  const OffscreenImagePtr texture_;
};

// This class creates an image that can be used as depth stencil attachment.
class DepthStencilImage : public Image {
 public:
  DepthStencilImage(const SharedBasicContext& context,
                    const VkExtent2D& extent);

  // This class is neither copyable nor movable.
  DepthStencilImage(const DepthStencilImage&) = delete;
  DepthStencilImage& operator=(const DepthStencilImage&) = delete;

 private:
  // Image buffer.
  DepthStencilBuffer buffer_;
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
};

// This class creates an image for multisampling.
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

  // Convenient function for creating a depth stencil image. Whether the image
  // is a multisample image depends on whether 'mode' has value.
  // Since we don't need to resolve multisampling depth stencil images, we can
  // directly use whatever image returned by this function.
  static std::unique_ptr<Image> CreateDepthStencilImage(
      SharedBasicContext context,
      const VkExtent2D& extent, absl::optional<Mode> mode);

  // This class is neither copyable nor movable.
  MultisampleImage(const MultisampleImage&) = delete;
  MultisampleImage& operator=(const MultisampleImage&) = delete;

  // Accessors.
  VkSampleCountFlagBits sample_count() const override { return sample_count_; }

 private:
  MultisampleImage(SharedBasicContext context,
                   const VkExtent2D& extent, VkFormat format,
                   Mode mode, MultisampleBuffer::Type type);

  // Returns the number of samples per pixel chosen according to 'mode' and
  // physical device limits.
  VkSampleCountFlagBits ChooseSampleCount(Mode mode);

  // Number of samples per pixel.
  const VkSampleCountFlagBits sample_count_;

  // Image buffer.
  MultisampleBuffer buffer_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_H */
