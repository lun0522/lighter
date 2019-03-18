//
//  image.h
//
//  Created by Pujun Lun on 3/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef WRAPPER_VULKAN_IMAGE_H
#define WRAPPER_VULKAN_IMAGE_H

#include <vector>

#include <vulkan/vulkan.hpp>

#include "buffer.h"

namespace wrapper {
namespace vulkan {

class Context;
class Image;

namespace image {

void BindImages(const std::vector<Image*>& images,
                const std::vector<uint32_t>& binding_points);

} /* namespace image */

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

} /* namespace vulkan */
} /* namespace wrapper */

#endif /* WRAPPER_VULKAN_IMAGE_H */
