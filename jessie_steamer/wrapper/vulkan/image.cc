//
//  image.cc
//
//  Created by Pujun Lun on 3/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/image.h"

#include <memory>
#include <stdexcept>

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/context.h"
#include "jessie_steamer/wrapper/vulkan/macro.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using common::Image;
using std::runtime_error;
using std::string;

VkImageView CreateImageView(const SharedContext& context,
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
      throw runtime_error{"Unsupported layer count: " +
                          std::to_string(layer_count)};
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

VkSampler CreateSampler(const SharedContext& context) {
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

void SwapChainImage::Init(const SharedContext& context,
                          const VkImage& image,
                          VkFormat format) {
  context_ = context;
  image_view_ = CreateImageView(context_, image, format,
                                VK_IMAGE_ASPECT_COLOR_BIT, /*layer_count=*/1);
}

SwapChainImage::~SwapChainImage() {
  // images are implicitly cleaned up with swapchain
  vkDestroyImageView(*context_->device(), image_view_, context_->allocator());
}

TextureImage::SharedTexture TextureImage::GetTexture(
    const SharedContext& context,
    const SourcePath& source_path) {
  const string* identifier;
  if (absl::holds_alternative<string>(source_path)) {
    identifier = &absl::get<string>(source_path);
  } else if (absl::holds_alternative<TextureImage::CubemapPath>(source_path)) {
    identifier = &absl::get<TextureImage::CubemapPath>(source_path).directory;
  } else {
    throw runtime_error{"Unrecognized variant type"};
  }
  return SharedTexture::Get(*identifier, context, source_path);
}

TextureImage::TextureImage(const SharedContext& context,
                           const SourcePath& source_path)
    : context_{context} {
  using CubemapImage = std::array<std::unique_ptr<Image>,
                                  buffer::kCubemapImageCount>;
  using SourceImage = absl::variant<std::unique_ptr<Image>, CubemapImage>;

  SourceImage source_image;
  const Image* sample_image;
  std::vector<const void*> datas;

  if (absl::holds_alternative<string>(source_path)) {
    auto& image = source_image.emplace<std::unique_ptr<Image>>(
        absl::make_unique<Image>(absl::get<string>(source_path)));
    sample_image = image.get();
    datas.emplace_back(image->data);
  } else if (absl::holds_alternative<CubemapPath>(source_path)) {
    const auto& cubemap_path = absl::get<CubemapPath>(source_path);
    auto& images = source_image.emplace<CubemapImage>();
    datas.resize(cubemap_path.files.size());
    for (int i = 0; i < cubemap_path.files.size(); ++i) {
      images[i] = absl::make_unique<Image>(absl::StrFormat(
          "%s/%s", cubemap_path.directory,cubemap_path.files[i]));
      datas[i] = images[i]->data;
    }
    sample_image = images[0].get();
  } else {
    throw runtime_error{"Unrecognized variant type"};
  }

  VkFormat format;
  switch (sample_image->channel) {
    case 1:
      format = VK_FORMAT_R8_UNORM;
      break;
    case 4:
      format = VK_FORMAT_R8G8B8A8_UNORM;
      break;
    default:
      throw runtime_error{"Unsupported number of channels: " +
                          std::to_string(sample_image->channel)};
  }

  TextureBuffer::Info image_info{
      absl::MakeSpan(datas),
      format,
      static_cast<uint32_t>(sample_image->width),
      static_cast<uint32_t>(sample_image->height),
      static_cast<uint32_t>(sample_image->channel),
  };
  buffer_.Init(context_, image_info);
  image_view_ = CreateImageView(context_, buffer_.image(), format,
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

TextureImage::~TextureImage() {
  vkDestroyImageView(*context_->device(), image_view_, context_->allocator());
  vkDestroySampler(*context_->device(), sampler_, context_->allocator());
}

void DepthStencilImage::Init(const SharedContext& context,
                             VkExtent2D extent) {
  context_ = context;
  buffer_.Init(context_, extent);
  image_view_ = CreateImageView(context_, buffer_.image(), format(),
                                VK_IMAGE_ASPECT_DEPTH_BIT
                                    | VK_IMAGE_ASPECT_STENCIL_BIT,
                                /*layer_count=*/1);
}

void DepthStencilImage::Cleanup() {
  vkDestroyImageView(*context_->device(), image_view_, context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
