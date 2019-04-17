//
//  image.cc
//
//  Created by Pujun Lun on 3/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/image.h"

#include <memory>
#include <stdexcept>

#include "jessie_steamer/wrapper/vulkan/context.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using common::util::Image;
using std::move;
using std::vector;

VkImageView CreateImageView(const SharedContext& context,
                            const VkImage& image,
                            VkImageViewType view_type,
                            VkFormat format,
                            VkImageAspectFlags aspect_mask,
                            uint32_t layer_count) {
  VkImageViewCreateInfo image_view_info{
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      /*pNext=*/nullptr,
      common::util::nullflag,
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
      common::util::nullflag,
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

void SwapChainImage::Init(SharedContext context,
                          const VkImage& image,
                          VkFormat format) {
  context_ = move(context);
  image_view_ = CreateImageView(context_, image, VK_IMAGE_VIEW_TYPE_2D, format,
                                VK_IMAGE_ASPECT_COLOR_BIT, /*layer_count=*/1);
}

SwapChainImage::~SwapChainImage() {
  // images are implicitly cleaned up with swapchain
  vkDestroyImageView(*context_->device(), image_view_, context_->allocator());
}

void TextureImage::Init(SharedContext context,
                        const vector<std::string>& paths) {
  vector<Image> images;
  images.reserve(paths.size());
  for (const auto& path : paths) {
    images.emplace_back(path);
  }
  Init(move(context), images);
}

void TextureImage::Init(SharedContext context,
                        const vector<Image>& images) {
  context_ = move(context);

  bool is_cubemap;
  switch (images.size()) {
    case 1:
      is_cubemap = false;
      break;
    case 6:
      is_cubemap = true;
      break;
    default:
      throw std::runtime_error{"Unsupported number of paths: " +
                               std::to_string(images.size())};
  }

  vector<const void*> datas(images.size());
  for (size_t i = 0; i < images.size(); ++i) {
    datas[i] = images[i].data;
  }

  VkFormat format;
  switch (images[0].channel) {
    case 1:
      format = VK_FORMAT_R8_UNORM;
      break;
    case 4:
      format = VK_FORMAT_R8G8B8A8_UNORM;
      break;
    default:
      throw std::runtime_error{"Unsupported number of channels: " +
                               std::to_string(images[0].channel)};
  }

  TextureBuffer::Info image_info{
     is_cubemap,
     {datas.begin(), datas.end()},
     format,
     static_cast<uint32_t>(images[0].width),
     static_cast<uint32_t>(images[0].height),
     static_cast<uint32_t>(images[0].channel),
  };
  buffer_.Init(context_, image_info);
  VkImageViewType view_type =
      is_cubemap ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
  image_view_ = CreateImageView(context_, buffer_.image(), view_type, format,
                                VK_IMAGE_ASPECT_COLOR_BIT,
                                CONTAINER_SIZE(images));
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

void DepthStencilImage::Init(SharedContext context,
                             VkExtent2D extent) {
  context_ = move(context);
  buffer_.Init(context_, extent);
  image_view_ = CreateImageView(context_, buffer_.image(),
                                VK_IMAGE_VIEW_TYPE_2D, format(),
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
