//
//  image.cc
//
//  Created by Pujun Lun on 3/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/image.h"

#include <memory>
#include <stdexcept>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/macro.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::runtime_error;

VkImageView CreateImageView(const SharedBasicContext& context,
                            const VkImage& image,
                            VkFormat format,
                            VkImageAspectFlags aspect_mask,
                            uint32_t layer_count) {
  VkImageViewType view_type;
  switch (layer_count) {
    case 1:
      view_type = VK_IMAGE_VIEW_TYPE_2D;
      break;
    case buffer::kCubemapImageCount:
      view_type = VK_IMAGE_VIEW_TYPE_CUBE;
      break;
    default:
      throw runtime_error{absl::StrFormat(
          "Unsupported layer count: %d", layer_count)};
  }

  VkImageViewCreateInfo image_view_info{
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      image,
      view_type,
      format,
      // enable swizzling color channels around
      VkComponentMapping{
          VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY
      },
      // specify image's purpose and which part to access
      VkImageSubresourceRange{
          /*aspectMask=*/aspect_mask,
          /*baseMipLevel=*/0,
          /*levelCount=*/1,
          /*baseArrayLayer=*/0,
          layer_count,
      },
  };

  VkImageView image_view;
  ASSERT_SUCCESS(vkCreateImageView(*context->device(), &image_view_info,
                                   context->allocator(), &image_view),
                 "Failed to create image view");
  return image_view;
}

VkSampler CreateSampler(const SharedBasicContext& context) {
  VkSamplerCreateInfo sampler_info{
      VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      /*magFilter=*/VK_FILTER_LINEAR,
      /*minFilter=*/VK_FILTER_LINEAR,
      /*mipmapMode=*/VK_SAMPLER_MIPMAP_MODE_LINEAR,
      /*addressModeU=*/VK_SAMPLER_ADDRESS_MODE_REPEAT,
      /*addressModeV=*/VK_SAMPLER_ADDRESS_MODE_REPEAT,
      /*addressModeW=*/VK_SAMPLER_ADDRESS_MODE_REPEAT,
      /*mipLodBias=*/0.0f,  // use for mipmapping
      /*anisotropyEnable=*/VK_TRUE,
      /*maxAnisotropy=*/16,  // max amount of texel samples used for anisotropy
      // may compare texels with a certain value and use result for filtering
      /*compareEnable=*/VK_FALSE,
      /*compareOp=*/VK_COMPARE_OP_ALWAYS,
      /*minLod=*/0.0f,  // use for mipmapping
      /*maxLod=*/0.0f,
      /*borderColor=*/VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      /*unnormalizedCoordinates=*/VK_FALSE,
  };

  VkSampler sampler;
  ASSERT_SUCCESS(vkCreateSampler(*context->device(), &sampler_info,
                                 context->allocator(), &sampler),
                 "Failed to create sampler");
  return sampler;
}

} /* namespace */

void SwapchainImage::Init(const VkImage& image, VkFormat format) {
  format_ = format;
  image_view_ = CreateImageView(context_, image, format_,
                                VK_IMAGE_ASPECT_COLOR_BIT, /*layer_count=*/1);
}

TextureImage::SharedTexture TextureImage::GetTexture(
    const SharedBasicContext& context, const SourcePath& source_path) {
  const std::string* identifier;
  if (absl::holds_alternative<SingleTexPath>(source_path)) {
    identifier = &absl::get<SingleTexPath>(source_path);
  } else if (absl::holds_alternative<TextureImage::CubemapPath>(source_path)) {
    identifier = &absl::get<TextureImage::CubemapPath>(source_path).directory;
  } else {
    throw runtime_error{"Unrecognized variant type"};
  }
  return SharedTexture::Get(*identifier, context, source_path);
}

TextureImage::TextureImage(SharedBasicContext context,
                           const SourcePath& source_path)
    : Image{std::move(context)}, buffer_{context_} {
  using RawImage = std::unique_ptr<common::Image>;
  using CubemapImage = std::array<RawImage, buffer::kCubemapImageCount>;
  using SourceImage =absl::variant<RawImage, CubemapImage>;

  SourceImage source_image;
  const common::Image* sample_image;
  std::vector<const void*> datas;

  if (absl::holds_alternative<SingleTexPath>(source_path)) {
    const auto& path = absl::get<SingleTexPath>(source_path);
    auto& image = source_image.emplace<RawImage>(
        absl::make_unique<common::Image>(path));
    sample_image = image.get();
    datas.emplace_back(image->data);
  } else if (absl::holds_alternative<CubemapPath>(source_path)) {
    const auto& cubemap_path = absl::get<CubemapPath>(source_path);
    auto& images = source_image.emplace<CubemapImage>();
    datas.resize(cubemap_path.files.size());
    for (int i = 0; i < cubemap_path.files.size(); ++i) {
      images[i] = absl::make_unique<common::Image>(absl::StrFormat(
          "%s/%s", cubemap_path.directory,cubemap_path.files[i]));
      datas[i] = images[i]->data;
    }
    sample_image = images[0].get();
  } else {
    throw runtime_error{"Unrecognized variant type"};
  }

  switch (sample_image->channel) {
    case 1:
      format_ = VK_FORMAT_R8_UNORM;
      break;
    case 4:
      format_ = VK_FORMAT_R8G8B8A8_UNORM;
      break;
    default:
      throw runtime_error{absl::StrFormat(
          "Unsupported number of channels: %d", sample_image->channel)};
  }

  TextureBuffer::Info image_info{
      absl::MakeSpan(datas),
      format_,
      static_cast<uint32_t>(sample_image->width),
      static_cast<uint32_t>(sample_image->height),
      static_cast<uint32_t>(sample_image->channel),
  };
  buffer_.Init(image_info);
  image_view_ = CreateImageView(context_, buffer_.image(), format_,
                                VK_IMAGE_ASPECT_COLOR_BIT,
                                /*layer_count=*/CONTAINER_SIZE(datas));
  sampler_ = CreateSampler(context_);
}

VkDescriptorImageInfo TextureImage::descriptor_info() const {
  return VkDescriptorImageInfo{
      sampler_,
      image_view_,
      /*imageLayout=*/VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
}

DepthStencilImage::DepthStencilImage(SharedBasicContext context,
                                     VkExtent2D extent)
    : Image{std::move(context)}, buffer_{context_, extent} {
  format_ = buffer_.format();
  image_view_ = CreateImageView(context_, buffer_.image(), format_,
                                VK_IMAGE_ASPECT_DEPTH_BIT
                                    | VK_IMAGE_ASPECT_STENCIL_BIT,
                                /*layer_count=*/1);
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
