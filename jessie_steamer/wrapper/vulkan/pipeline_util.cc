//
//  pipeline_util.cc
//
//  Created by Pujun Lun on 6/19/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"

#include "jessie_steamer/common/file.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace pipeline {
namespace {

using common::Vertex2D;
using common::Vertex3DNoTex;
using common::Vertex3DWithTex;
using std::vector;

using VertexAttribute = VertexBuffer::Attribute;

} /* namespace */

VkPipelineColorBlendAttachmentState GetColorBlendState(bool enable_blend) {
  return VkPipelineColorBlendAttachmentState{
      /*blendEnable=*/util::ToVkBool(enable_blend),
      /*srcColorBlendFactor=*/VK_BLEND_FACTOR_SRC_ALPHA,
      /*dstColorBlendFactor=*/VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      /*colorBlendOp=*/VK_BLEND_OP_ADD,
      /*srcAlphaBlendFactor=*/VK_BLEND_FACTOR_ONE,
      /*dstAlphaBlendFactor=*/VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      /*alphaBlendOp=*/VK_BLEND_OP_ADD,
      /*colorWriteMask=*/
      VK_COLOR_COMPONENT_R_BIT
          | VK_COLOR_COMPONENT_G_BIT
          | VK_COLOR_COMPONENT_B_BIT
          | VK_COLOR_COMPONENT_A_BIT,
  };
}

vector<VkVertexInputBindingDescription> GetBindingDescriptions(
    const vector<VertexInputBinding>& bindings) {
  vector<VkVertexInputBindingDescription> descriptions;
  descriptions.reserve(bindings.size());
  for (const auto& binding : bindings) {
    descriptions.emplace_back(VkVertexInputBindingDescription{
        binding.binding_point,
        binding.data_size,
        binding.instancing ? VK_VERTEX_INPUT_RATE_INSTANCE
                           : VK_VERTEX_INPUT_RATE_VERTEX,
    });
  }
  return descriptions;
}

template <>
vector<VertexAttribute> GetVertexAttribute<Vertex2D>() {
  return {
      VertexAttribute{
          /*location=*/0,  // To be updated.
          /*offset=*/static_cast<uint32_t>(offsetof(Vertex2D, pos)),
          VK_FORMAT_R32G32_SFLOAT,
      },
      VertexAttribute{
          /*location=*/0,  // To be updated.
          /*offset=*/static_cast<uint32_t>(offsetof(Vertex2D, tex_coord)),
          VK_FORMAT_R32G32_SFLOAT,
      },
  };
}

template <>
vector<VertexAttribute> GetVertexAttribute<Vertex3DNoTex>() {
  return {
      VertexAttribute{
          /*location=*/0,  // To be updated.
          /*offset=*/static_cast<uint32_t>(offsetof(Vertex3DNoTex, pos)),
          VK_FORMAT_R32G32B32_SFLOAT,
      },
      VertexAttribute{
          /*location=*/0,  // To be updated.
          /*offset=*/static_cast<uint32_t>(offsetof(Vertex3DNoTex, color)),
          VK_FORMAT_R32G32B32_SFLOAT,
      },
  };
}

template <>
vector<VertexAttribute> GetVertexAttribute<Vertex3DWithTex>() {
  return {
      VertexAttribute{
          /*location=*/0,  // To be updated.
          /*offset=*/static_cast<uint32_t>(offsetof(Vertex3DWithTex, pos)),
          VK_FORMAT_R32G32B32_SFLOAT,
      },
      VertexAttribute{
          /*location=*/0,  // To be updated.
          /*offset=*/static_cast<uint32_t>(offsetof(Vertex3DWithTex, norm)),
          VK_FORMAT_R32G32B32_SFLOAT,
      },
      VertexAttribute{
          /*location=*/0,  // To be updated.
          /*offset=*/
          static_cast<uint32_t>(offsetof(Vertex3DWithTex, tex_coord)),
          VK_FORMAT_R32G32_SFLOAT,
      },
  };
}

vector<VkVertexInputAttributeDescription> GetAttributeDescriptions(
    uint32_t binding_point, const vector<VertexAttribute>& attributes) {
  vector<VkVertexInputAttributeDescription> descriptions;
  descriptions.reserve(attributes.size());
  for (const auto& attribute : attributes) {
    descriptions.emplace_back(VkVertexInputAttributeDescription{
        attribute.location,
        binding_point,
        attribute.format,
        attribute.offset,
    });
  }
  return descriptions;
}

} /* namespace pipeline */
} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
