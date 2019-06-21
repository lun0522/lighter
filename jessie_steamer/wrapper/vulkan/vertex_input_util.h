//
//  vertex_input_util.h
//
//  Created by Pujun Lun on 6/19/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_VERTEX_INPUT_UTIL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_VERTEX_INPUT_UTIL_H

#include <vector>

#include "jessie_steamer/wrapper/vulkan/util.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

/* Vertex input binding description */

struct VertexInputBinding {
  uint32_t binding_point;
  uint32_t data_size;
  bool instancing;
};

template <typename VertexType>
VertexInputBinding GetPerVertexBindings() {
  return VertexInputBinding{
      kPerVertexBindingPoint,
      /*data_size=*/static_cast<uint32_t>(sizeof(VertexType)),
      /*instancing=*/false,
  };
}

std::vector<VkVertexInputBindingDescription> GetBindingDescriptions(
    const std::vector<VertexInputBinding>& bindings);

/* Vertex input attribute description */

struct VertexAttribute {
  uint32_t location;
  uint32_t offset;
  VkFormat format;
};

struct VertexInputAttribute {
  uint32_t binding_point;
  std::vector<VertexAttribute> attributes;
};

template <typename VertexType>
VertexInputAttribute GetVertexAttributes();

std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions(
    const std::vector<VertexInputAttribute>& attributes);

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_VERTEX_INPUT_UTIL_H */
