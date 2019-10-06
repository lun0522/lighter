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

using common::VertexAttribute2D;
using common::VertexAttribute3D;
using std::vector;

using VertexAttribute = VertexInputAttribute::Attribute;

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
VertexInputAttribute GetPerVertexAttribute<VertexAttribute2D>() {
  return VertexInputAttribute{
      kPerVertexBindingPoint,
      /*attributes=*/{
          VertexAttribute{
              /*location=*/0,
              /*offset=*/
              static_cast<uint32_t>(offsetof(VertexAttribute2D, pos)),
              VK_FORMAT_R32G32_SFLOAT,
          },
          VertexAttribute{
              /*location=*/1,
              /*offset=*/
              static_cast<uint32_t>(offsetof(VertexAttribute2D, tex_coord)),
              VK_FORMAT_R32G32_SFLOAT,
          },
      },
  };
}

template <>
VertexInputAttribute GetPerVertexAttribute<VertexAttribute3D>() {
  return VertexInputAttribute{
      kPerVertexBindingPoint,
      /*attributes=*/{
          VertexAttribute{
              /*location=*/0,
              /*offset=*/
              static_cast<uint32_t>(offsetof(VertexAttribute3D, pos)),
              VK_FORMAT_R32G32B32_SFLOAT,
          },
          VertexAttribute{
              /*location=*/1,
              /*offset=*/
              static_cast<uint32_t>(offsetof(VertexAttribute3D, norm)),
              VK_FORMAT_R32G32B32_SFLOAT,
          },
          VertexAttribute{
              /*location=*/2,
              /*offset=*/
              static_cast<uint32_t>(offsetof(VertexAttribute3D, tex_coord)),
              VK_FORMAT_R32G32_SFLOAT,
          },
      },
  };
}

vector<VkVertexInputAttributeDescription> GetAttributeDescriptions(
    const vector<VertexInputAttribute>& attributes) {
  int num_attributes = 0;
  for (const auto& attribs : attributes) {
    num_attributes += attribs.attributes.size();
  }

  vector<VkVertexInputAttributeDescription> descriptions;
  descriptions.reserve(num_attributes);
  for (const auto& attribs : attributes) {
    for (const auto& attrib : attribs.attributes) {
      descriptions.emplace_back(VkVertexInputAttributeDescription{
          attrib.location,
          attribs.binding_point,
          attrib.format,
          attrib.offset,
      });
    }
  }
  return descriptions;
}

} /* namespace pipeline */
} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
