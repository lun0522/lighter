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

class Image {
 public:
  Image() = default;
  void Init(std::shared_ptr<Context> context,
            const std::string& path);
  ~Image();

  // This class is neither copyable nor movable
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  VkDescriptorImageInfo descriptor_info() const;

 private:
  std::shared_ptr<Context> context_;
  ImageBuffer image_buffer_;
  VkImageView image_view_;
  VkSampler sampler_;
};

class Images {
 public:
  Images() = default;
  void Init(std::shared_ptr<Context> context,
            const std::vector<std::string>& paths,
            const std::vector<uint32_t>& binding_points,
            VkShaderStageFlags shader_stage);

  // This class is neither copyable nor movable
  Images(const Images&) = delete;
  Images& operator=(const Images&) = delete;

  const Descriptor& descriptor() const { return descriptor_; }

 private:
  std::vector<std::unique_ptr<Image>> images_;
  Descriptor descriptor_;
};

} /* namespace vulkan */
} /* namespace wrapper */

#endif /* WRAPPER_VULKAN_IMAGE_H */
