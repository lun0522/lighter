//
//  image.cc
//
//  Created by Pujun Lun on 3/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#include <image/stb_image.h>

#include "context.h"
#include "util.h"

namespace wrapper {
namespace vulkan {
namespace {

VkImageView CreateImageView(SharedContext context,
                            const VkImage& image,
                            VkFormat format) {
  VkImageViewCreateInfo image_view_info{
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      image,
      /*viewType=*/VK_IMAGE_VIEW_TYPE_2D,  // 2D, 3D, cube maps
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
          /*aspectMask=*/VK_IMAGE_ASPECT_COLOR_BIT,
          /*baseMipLevel=*/0,
          /*levelCount=*/1,
          /*baseArrayLayer=*/0,
          /*layerCount=*/1,
      },
  };

  VkImageView image_view;
  ASSERT_SUCCESS(vkCreateImageView(*context->device(), &image_view_info,
                                   context->allocator(), &image_view),
                 "Failed to create image view");
  return image_view;
}

VkSampler CreateSampler(SharedContext context) {
  VkSamplerCreateInfo sampler_info{
      VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
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

void Image::Init(SharedContext context,
                 const std::string& path) {
  context_ = context;

  int width, height, channel;
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;  // TODO: other formats
  stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channel,
                            STBI_rgb_alpha);  // force to have alpha
  image_buffer_.Init(context_, {data, format, static_cast<uint32_t>(width),
                                static_cast<uint32_t>(height), 4});
  stbi_image_free(data);

  image_view_ = CreateImageView(context_, image_buffer_.image(), format);
  sampler_ = CreateSampler(context_);
}

Image::~Image() {
  vkDestroySampler(*context_->device(), sampler_, context_->allocator());
  vkDestroyImageView(*context_->device(), image_view_, context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */

