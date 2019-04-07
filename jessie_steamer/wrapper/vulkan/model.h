//
//  model.h
//
//  Created by Pujun Lun on 3/22/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H

#include <string>
#include <vector>

#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "third_party/vulkan/vulkan.h"

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

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H */
