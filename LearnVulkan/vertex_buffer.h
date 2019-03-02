//
//  vertex_buffer.h
//  LearnVulkan
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LEARNVULKAN_VERTEX_BUFFER_H
#define LEARNVULKAN_VERTEX_BUFFER_H

#include <vulkan/vulkan.hpp>

#include "util.h"

namespace vulkan {

class Application;

class VertexBuffer { // allocate space on device; doesn't depend on swap chain
 public:
  VertexBuffer(const Application& app) : app_{app} {}
  void Init(const void* vertex_data, size_t vertex_size, size_t vertex_count,
            const void*  index_data, size_t  index_size, size_t  index_count);
  void Draw(const VkCommandBuffer& command_buffer) const;
  ~VertexBuffer();
  MARK_NOT_COPYABLE_OR_MOVABLE(VertexBuffer);

 private:
  const Application &app_;
  VkBuffer buffer_;
  VkDeviceMemory device_memory_;
  VkDeviceSize vertex_size_;
  uint32_t vertex_count_, index_count_;
};

} /* namespace vulkan */

#endif /* LEARNVULKAN_VERTEX_BUFFER_H */
