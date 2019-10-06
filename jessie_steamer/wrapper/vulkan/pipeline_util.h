//
//  pipeline_util.h
//
//  Created by Pujun Lun on 6/19/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_UTIL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_UTIL_H

#include <vector>

#include "jessie_steamer/wrapper/vulkan/util.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace pipeline {

/* Color blend */

// Returns the color blend state that gives:
//   C = Cs * As + Cd * (1. - As)
//   A = As * 1. + Ad * (1. - As)
// Where: C - color, A - alpha, s - source, d - destination.
VkPipelineColorBlendAttachmentState GetColorBlendState(bool enable_blend);

/* Vertex input binding */

// Specifies that at 'binding_point', each vertex will get data of 'data_size'.
// 'instancing' determines whether to update data per-instance or per-vertex.
// Note that the binding point is not a binding number in the shader, but the
// vertex buffer binding point used in vkCmdBindVertexBuffers(),
struct VertexInputBinding {
  uint32_t binding_point;
  uint32_t data_size;
  bool instancing;
};

// Convenient function to return an instance of VertexInputBinding, assuming
// each vertex will get data of DataType, which is updated per-vertex.
template <typename DataType>
VertexInputBinding GetPerVertexBinding() {
  return VertexInputBinding{
      kPerVertexBindingPoint,
      /*data_size=*/static_cast<uint32_t>(sizeof(DataType)),
      /*instancing=*/false,
  };
}

// Converts VertexInputBinding to VkVertexInputBindingDescription.
std::vector<VkVertexInputBindingDescription> GetBindingDescriptions(
    const std::vector<VertexInputBinding>& bindings);

/* Vertex input attribute */

// Interprets the data bound to 'binding_point'.
// Note that the binding point is not a binding number in the shader, but the
// vertex buffer binding point used in vkCmdBindVertexBuffers(),
struct VertexInputAttribute {
  struct Attribute {
    uint32_t location;
    uint32_t offset;
    VkFormat format;
  };

  uint32_t binding_point;
  std::vector<Attribute> attributes;
};

// Convenient function to return an instance of VertexInputAttribute, assuming
// each vertex will get data of DataType, which is updated per-vertex.
// For now this is only implemented for VertexAttribute2D and VertexAttribute3D.
template <typename DataType>
VertexInputAttribute GetPerVertexAttribute();

// Converts VertexInputAttribute to VkVertexInputAttributeDescription.
std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions(
    const std::vector<VertexInputAttribute>& attributes);

} /* namespace pipeline */
} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_UTIL_H */
