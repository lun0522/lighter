//
//  image.cc
//
//  Created by Pujun Lun on 3/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/image.h"

#include <algorithm>
#include <vector>

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/common/util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::vector;

// Returns the image format to use for a color image given number of 'channel'.
// Only 1 or 4 channels are supported.
VkFormat FindColorImageFormat(int channel) {
  switch (channel) {
    case 1:
      return VK_FORMAT_R8_UNORM;
    case 4:
      return VK_FORMAT_R8G8B8A8_UNORM;
    default:
      FATAL(absl::StrFormat(
          "Number of channels can only be 1 or 4, while %d provided", channel));
  }
}

// Returns image format with specified 'features', and throws a runtime
// exception if not found.
VkFormat FindImageFormatWithFeature(const SharedBasicContext& context,
                                    const vector<VkFormat>& candidates,
                                    VkFormatFeatureFlags features) {
  for (auto format : candidates) {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(*context->physical_device(),
                                        format, &properties);
    if ((properties.optimalTilingFeatures & features) == features) {
      return format;
    }
  }
  FATAL("Failed to find suitable image format");
}

// Returns the image format to use for a depth stencil image.
VkFormat FindDepthStencilImageFormat(const SharedBasicContext& context) {
  return FindImageFormatWithFeature(
      context,
      {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT},
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

// Returns the maximum number of samples per pixel indicated by 'sample_counts'.
VkSampleCountFlagBits GetMaxSampleCount(VkSampleCountFlags sample_counts) {
  for (auto count : {VK_SAMPLE_COUNT_64_BIT,
                     VK_SAMPLE_COUNT_32_BIT,
                     VK_SAMPLE_COUNT_16_BIT,
                     VK_SAMPLE_COUNT_8_BIT,
                     VK_SAMPLE_COUNT_4_BIT,
                     VK_SAMPLE_COUNT_2_BIT}) {
    if (sample_counts & count) {
      return count;
    }
  }
  FATAL("Multisampling is not supported by hardware");
}

// Creates an image view to specify the usage of image data.
VkImageView CreateImageView(const SharedBasicContext& context,
                            const VkImage& image,
                            VkFormat format,
                            VkImageAspectFlags image_aspect,
                            uint32_t mip_levels,
                            uint32_t layer_count) {
  VkImageViewType view_type;
  switch (layer_count) {
    case 1:
      view_type = VK_IMAGE_VIEW_TYPE_2D;
      break;
    case kCubemapImageCount:
      view_type = VK_IMAGE_VIEW_TYPE_CUBE;
      break;
    default:
      FATAL(absl::StrFormat("Unsupported layer count: %d", layer_count));
  }

  const VkImageViewCreateInfo image_view_info{
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      image,
      view_type,
      format,
      // Swizzle color channels around.
      VkComponentMapping{
          VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY
      },
      // Specify image's purpose and which part to access.
      VkImageSubresourceRange{
          image_aspect,
          /*baseMipLevel=*/0,
          mip_levels,
          /*baseArrayLayer=*/0,
          layer_count,
      },
  };

  VkImageView image_view;
  ASSERT_SUCCESS(vkCreateImageView(*context->device(), &image_view_info,
                                   *context->allocator(), &image_view),
                 "Failed to create image view");
  return image_view;
}

// Creates an image sampler.
VkSampler CreateSampler(const SharedBasicContext& context,
                        int mip_levels, const SamplableImage::Config& config) {
  // 'mipLodBias', 'minLod' and 'maxLod' are used to control mipmapping.
  const VkSamplerCreateInfo sampler_info{
      VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      /*magFilter=*/config.filter,
      /*minFilter=*/config.filter,
      VK_SAMPLER_MIPMAP_MODE_LINEAR,
      /*addressModeU=*/config.address_mode,
      /*addressModeV=*/config.address_mode,
      /*addressModeW=*/config.address_mode,
      /*mipLodBias=*/0.0f,
      /*anisotropyEnable=*/VK_TRUE,
      // Max amount of texel samples used for anisotropy.
      /*maxAnisotropy=*/16,
      // May compare texels with a certain value and use result for filtering.
      /*compareEnable=*/VK_FALSE,
      /*compareOp=*/VK_COMPARE_OP_ALWAYS,
      /*minLod=*/0.0f,
      /*maxLod=*/static_cast<float>(mip_levels),
      VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      /*unnormalizedCoordinates=*/VK_FALSE,
  };

  VkSampler sampler;
  ASSERT_SUCCESS(vkCreateSampler(*context->device(), &sampler_info,
                                 *context->allocator(), &sampler),
                 "Failed to create sampler");
  return sampler;
}

} /* namespace */

TextureImage::TextureImage(SharedBasicContext context,
                           bool generate_mipmaps,
                           const SamplableImage::Config& sampler_config,
                           const TextureBuffer::Info& info)
    : Image{std::move(context), info.GetExtent2D(), info.format},
      buffer_{context_, generate_mipmaps, info},
      sampler_{CreateSampler(context_, buffer_.mip_levels(), sampler_config)} {
  image_view_ = CreateImageView(context_, buffer_.image(), format_,
                                VK_IMAGE_ASPECT_COLOR_BIT, buffer_.mip_levels(),
                                /*layer_count=*/CONTAINER_SIZE(info.datas));
}

VkDescriptorImageInfo TextureImage::GetDescriptorInfo() const {
  return VkDescriptorImageInfo{
      sampler_,
      image_view_,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
}

SharedTexture::RefCountedTexture SharedTexture::GetTexture(
    SharedBasicContext context, const SourcePath& source_path,
    const SamplableImage::Config& sampler_config) {
  context->RegisterRefCountPool<SharedTexture::RefCountedTexture>();

  using SingleImage = std::unique_ptr<common::Image>;
  using CubemapImage = std::array<SingleImage, kCubemapImageCount>;
  using SourceImage = absl::variant<SingleImage, CubemapImage>;

  bool generate_mipmaps;
  const std::string* identifier;
  SourceImage source_image;
  const common::Image* sample_image;
  vector<const void*> datas;

  if (absl::holds_alternative<SingleTexPath>(source_path)) {
    generate_mipmaps = true;
    const auto& single_tex_path = absl::get<SingleTexPath>(source_path);
    identifier = &single_tex_path;
    auto& image = source_image.emplace<SingleImage>(
        absl::make_unique<common::Image>(single_tex_path));
    sample_image = image.get();
    datas.emplace_back(image->data);
  } else if (absl::holds_alternative<CubemapPath>(source_path)) {
    generate_mipmaps = false;
    const auto& cubemap_path = absl::get<CubemapPath>(source_path);
    identifier = &cubemap_path.directory;
    auto& images = source_image.emplace<CubemapImage>();
    datas.resize(cubemap_path.files.size());
    for (int i = 0; i < cubemap_path.files.size(); ++i) {
      images[i] = absl::make_unique<common::Image>(absl::StrFormat(
          "%s/%s", cubemap_path.directory, cubemap_path.files[i]));
      datas[i] = images[i]->data;
    }
    sample_image = images[0].get();
  } else {
    FATAL("Unrecognized variant type");
  }

  return RefCountedTexture::Get(
      *identifier, std::move(context), generate_mipmaps, sampler_config,
      TextureBuffer::Info{
          std::move(datas),
          FindColorImageFormat(sample_image->channel),
          static_cast<uint32_t>(sample_image->width),
          static_cast<uint32_t>(sample_image->height),
          static_cast<uint32_t>(sample_image->channel),
      });
}

OffscreenImage::OffscreenImage(SharedBasicContext context,
                               int channel, const VkExtent2D& extent,
                               const SamplableImage::Config& sampler_config)
    : Image{std::move(context), extent, FindColorImageFormat(channel)},
      buffer_{context_, extent_, format_},
      sampler_{CreateSampler(context_, kSingleMipLevel, sampler_config)} {
  image_view_ = CreateImageView(context_, buffer_.image(), format_,
                                VK_IMAGE_ASPECT_COLOR_BIT,
                                kSingleMipLevel, kSingleImageLayer);
}

VkDescriptorImageInfo OffscreenImage::GetDescriptorInfo() const {
  return VkDescriptorImageInfo{
      sampler_,
      image_view_,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
}

DepthStencilImage::DepthStencilImage(const SharedBasicContext& context,
                                     const VkExtent2D& extent)
    : Image{context, extent, FindDepthStencilImageFormat(context)},
      buffer_{context_, extent_, format_} {
  image_view_ = CreateImageView(context_, buffer_.image(), format_,
                                VK_IMAGE_ASPECT_DEPTH_BIT
                                    | VK_IMAGE_ASPECT_STENCIL_BIT,
                                kSingleMipLevel, kSingleImageLayer);
}

SwapchainImage::SwapchainImage(SharedBasicContext context,
                               const VkImage& image,
                               const VkExtent2D& extent, VkFormat format)
    : Image{std::move(context), extent, format} {
  image_view_ = CreateImageView(context_, image, format_,
                                VK_IMAGE_ASPECT_COLOR_BIT,
                                kSingleMipLevel, kSingleImageLayer);
}

std::unique_ptr<MultisampleImage> MultisampleImage::CreateColorMultisampleImage(
    SharedBasicContext context,
    const Image& target_image, Mode mode) {
  return std::unique_ptr<MultisampleImage>(
      new MultisampleImage{std::move(context), target_image.extent(),
                           target_image.format(),
                           mode, MultisampleBuffer::Type::kColor});
}

std::unique_ptr<MultisampleImage>
MultisampleImage::CreateDepthStencilMultisampleImage(
    SharedBasicContext context,
    const VkExtent2D& extent, Mode mode) {
  const VkFormat format = FindDepthStencilImageFormat(context);
  return std::unique_ptr<MultisampleImage>(
      new MultisampleImage{std::move(context), extent, format,
                           mode, MultisampleBuffer::Type::kDepthStencil});
}

std::unique_ptr<Image> MultisampleImage::CreateDepthStencilImage(
    SharedBasicContext context,
    const VkExtent2D& extent, const absl::optional<Mode>& mode) {
  if (mode.has_value()) {
    return CreateDepthStencilMultisampleImage(std::move(context),
                                              extent, mode.value());
  } else {
    return absl::make_unique<DepthStencilImage>(std::move(context), extent);
  }
}

MultisampleImage::MultisampleImage(
    SharedBasicContext context,
    const VkExtent2D& extent, VkFormat format,
    Mode mode, MultisampleBuffer::Type type)
    : Image{std::move(context), extent, format},
      sample_count_{ChooseSampleCount(mode)},
      buffer_{context_, type, extent_, format_, sample_count_} {
  VkImageAspectFlags image_aspect;
  switch (type) {
    case MultisampleBuffer::Type::kColor:
      image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
      break;
    case MultisampleBuffer::Type::kDepthStencil:
      image_aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
  }
  image_view_ = CreateImageView(
      context_, buffer_.image(), format_,
      image_aspect, kSingleMipLevel, kSingleImageLayer);
}

VkSampleCountFlagBits MultisampleImage::ChooseSampleCount(Mode mode) {
  const auto& limits = context_->physical_device_limits();
  const VkSampleCountFlags sample_count_flag = std::min({
      limits.framebufferColorSampleCounts,
      limits.framebufferDepthSampleCounts,
      limits.framebufferStencilSampleCounts,
  });
  const VkSampleCountFlagBits max_sample_count =
      GetMaxSampleCount(sample_count_flag);
  switch (mode) {
    case Mode::kEfficient:
      return std::min(VK_SAMPLE_COUNT_4_BIT, max_sample_count);
    case Mode::kBestEffect:
      return max_sample_count;
  }
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
