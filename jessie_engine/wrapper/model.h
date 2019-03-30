//
//  model.h
//
//  Created by Pujun Lun on 3/22/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef WRAPPER_VULKAN_MODEL_H
#define WRAPPER_VULKAN_MODEL_H

#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "buffer.h"

namespace wrapper {
namespace vulkan {

class Context;

class Model {
 public:
  Model() = default;
  void Init(std::shared_ptr<Context> context,
            const std::string& path,
            int index_base);
  void Draw(const VkCommandBuffer& command_buffer) const {
    vertex_buffer_.Draw(command_buffer);
  }

  // This class is neither copyable nor movable
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  static const std::vector<VkVertexInputBindingDescription>& binding_descs();
  static const std::vector<VkVertexInputAttributeDescription>& attrib_descs();

 private:
  VertexBuffer vertex_buffer_;
};

} /* namespace vulkan */
} /* namespace wrapper */

#endif /* WRAPPER_VULKAN_MODEL_H */
