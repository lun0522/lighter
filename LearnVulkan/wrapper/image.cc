//
//  image.cc
//
//  Created by Pujun Lun on 3/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "image.h"

#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <image/stb_image.h>

#include "context.h"
#include "util.h"

namespace wrapper {
namespace vulkan {
namespace {

using std::vector;

VkImageView CreateImageView(SharedContext context,
                            const VkImage& image,
                            VkImageViewType view_type,
                            VkFormat format,
                            VkImageAspectFlags aspect_mask,
                            uint32_t layer_count) {
  VkImageViewCreateInfo image_view_info{
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
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

void SwapChainImage::Init(SharedContext context,
                          const VkImage& image,
                          VkFormat format) {
  context_ = context;
  image_view_ = CreateImageView(context_, image, VK_IMAGE_VIEW_TYPE_2D, format,
                                VK_IMAGE_ASPECT_COLOR_BIT, /*layer_count=*/1);
}

SwapChainImage::~SwapChainImage() {
  // images are implicitly cleaned up with swapchain
  vkDestroyImageView(*context_->device(), image_view_, context_->allocator());
}

void TextureImage::Init(std::shared_ptr<Context> context,
                        const vector<std::string>& paths) {
  context_ = context;

  bool is_cube_map;
  switch (paths.size()) {
    case 1:
      is_cube_map = false;
      break;
    case 6:
      is_cube_map = true;
      break;
    default:
      throw std::runtime_error{"Wrong number of paths: " +
                               std::to_string(paths.size())};
  }
  int width = 0, height = 0, channel = 0;
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;  // TODO: other formats

  std::vector<void*> datas;
  datas.reserve(paths.size());
  for (const auto& path : paths) {
    datas.emplace_back(stbi_load(path.c_str(), &width, &height, &channel,
                                 STBI_rgb_alpha));  // force to have alpha
  }
  buffer_.Init(context_, {is_cube_map, {datas.begin(), datas.end()},
                          format, static_cast<uint32_t>(width),
                          static_cast<uint32_t>(height), 4});
  for (auto* data : datas) {
    stbi_image_free(data);
  }

  VkImageViewType view_type =
      is_cube_map ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
  image_view_ = CreateImageView(context_, buffer_.image(), view_type, format,
                                VK_IMAGE_ASPECT_COLOR_BIT,
                                CONTAINER_SIZE(paths));
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
  context_ = context;
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

