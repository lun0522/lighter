//
//  pipeline_util.cc
//
//  Created by Pujun Lun on 6/19/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/common/util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace pipeline {
namespace {

using common::Vertex2D;
using common::Vertex3DPosOnly;
using common::Vertex3DWithColor;
using common::Vertex3DWithTex;
using std::vector;

using VertexAttribute = VertexBuffer::Attribute;

} /* namespace */

GraphicsPipelineBuilder::ViewportInfo GetFullFrameViewport(
    const VkExtent2D& frame_size) {
  return GraphicsPipelineBuilder::ViewportInfo{
      /*viewport=*/VkViewport{
          /*x=*/0.0f,
          /*y=*/0.0f,
          static_cast<float>(frame_size.width),
          static_cast<float>(frame_size.height),
          /*minDepth=*/0.0f,
          /*maxDepth=*/1.0f,
      },
      /*scissor=*/VkRect2D{
          /*offset=*/{0, 0},
          frame_size,
      }
  };
}

GraphicsPipelineBuilder::ViewportInfo GetViewport(const VkExtent2D& frame_size,
                                                  float aspect_ratio) {
  // Do not use unsigned numbers for subtraction.
  const glm::ivec2 current_size{frame_size.width, frame_size.height};
  const glm::ivec2 effective_size =
      common::util::FindLargestExtent(current_size, aspect_ratio);
  return GraphicsPipelineBuilder::ViewportInfo{
      /*viewport=*/VkViewport{
          /*x=*/static_cast<float>(current_size.x - effective_size.x) / 2.0f,
          /*y=*/static_cast<float>(current_size.y - effective_size.y) / 2.0f,
          static_cast<float>(effective_size.x),
          static_cast<float>(effective_size.y),
          /*minDepth=*/0.0f,
          /*maxDepth=*/1.0f,
      },
      /*scissor=*/VkRect2D{
          /*offset=*/{0, 0},
          frame_size,
      }
  };
}

VkPipelineColorBlendAttachmentState GetColorBlendState(bool enable_blend) {
  return VkPipelineColorBlendAttachmentState{
      /*blendEnable=*/util::ToVkBool(enable_blend),
      /*srcColorBlendFactor=*/VK_BLEND_FACTOR_SRC_COLOR,
      /*dstColorBlendFactor=*/VK_BLEND_FACTOR_DST_COLOR,
      /*colorBlendOp=*/VK_BLEND_OP_ADD,
      /*srcAlphaBlendFactor=*/VK_BLEND_FACTOR_ONE,
      /*dstAlphaBlendFactor=*/VK_BLEND_FACTOR_ONE,
      /*alphaBlendOp=*/VK_BLEND_OP_ADD,
      /*colorWriteMask=*/
      VK_COLOR_COMPONENT_R_BIT
          | VK_COLOR_COMPONENT_G_BIT
          | VK_COLOR_COMPONENT_B_BIT
          | VK_COLOR_COMPONENT_A_BIT,
  };
}

VkPipelineColorBlendAttachmentState GetColorAlphaBlendState(bool enable_blend) {
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

VkVertexInputBindingDescription GetBindingDescription(uint32_t stride,
                                                      bool instancing) {
  return VkVertexInputBindingDescription{
      /*binding=*/0,  // To be updated.
      stride,
      instancing ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX,
  };
}

template <>
vector<VertexAttribute> GetVertexAttribute<Vertex2D>() {
  return {
      VertexAttribute{
          /*offset=*/static_cast<uint32_t>(offsetof(Vertex2D, pos)),
          VK_FORMAT_R32G32_SFLOAT,
      },
      VertexAttribute{
          /*offset=*/static_cast<uint32_t>(offsetof(Vertex2D, tex_coord)),
          VK_FORMAT_R32G32_SFLOAT,
      },
  };
}

template <>
vector<VertexAttribute> GetVertexAttribute<Vertex3DPosOnly>() {
  return {
      VertexAttribute{
          /*offset=*/static_cast<uint32_t>(offsetof(Vertex3DWithColor, pos)),
          VK_FORMAT_R32G32B32_SFLOAT,
      },
  };
}

template <>
vector<VertexAttribute> GetVertexAttribute<Vertex3DWithColor>() {
  return {
      VertexAttribute{
          /*offset=*/static_cast<uint32_t>(offsetof(Vertex3DWithColor, pos)),
          VK_FORMAT_R32G32B32_SFLOAT,
      },
      VertexAttribute{
          /*offset=*/static_cast<uint32_t>(offsetof(Vertex3DWithColor, color)),
          VK_FORMAT_R32G32B32_SFLOAT,
      },
  };
}

template <>
vector<VertexAttribute> GetVertexAttribute<Vertex3DWithTex>() {
  return {
      VertexAttribute{
          /*offset=*/static_cast<uint32_t>(offsetof(Vertex3DWithTex, pos)),
          VK_FORMAT_R32G32B32_SFLOAT,
      },
      VertexAttribute{
          /*offset=*/static_cast<uint32_t>(offsetof(Vertex3DWithTex, norm)),
          VK_FORMAT_R32G32B32_SFLOAT,
      },
      VertexAttribute{
          /*offset=*/
          static_cast<uint32_t>(offsetof(Vertex3DWithTex, tex_coord)),
          VK_FORMAT_R32G32_SFLOAT,
      },
  };
}

} /* namespace pipeline */
} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
