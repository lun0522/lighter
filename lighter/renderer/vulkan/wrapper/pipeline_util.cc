//
//  pipeline_util.cc
//
//  Created by Pujun Lun on 6/19/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/pipeline_util.h"

#include <algorithm>
#include <iterator>

#include "lighter/common/util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace pipeline {
namespace {

// Returns the format to use for 'attribute'.
VkFormat ChooseFormat(common::VertexAttribute attribute) {
  if (attribute.data_type != common::VertexAttribute::DataType::kFloat) {
    FATAL("Can only handle float");
  }

  switch (attribute.length) {
    case 1:
      return VK_FORMAT_R32_SFLOAT;
    case 2:
      return VK_FORMAT_R32G32_SFLOAT;
    case 3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case 4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    default:
      FATAL(absl::StrFormat("Length must be in range [1, 4], while %d provided",
                            attribute.length));
  }
}

} /* namespace */

VkStencilOpState GetStencilReadOpState(VkCompareOp compare_op,
                                       uint32_t reference) {
  return VkStencilOpState{
      /*failOp=*/VK_STENCIL_OP_KEEP,
      /*passOp=*/VK_STENCIL_OP_KEEP,
      /*depthFailOp=*/VK_STENCIL_OP_KEEP,
      compare_op,
      /*compareMask=*/0xFF,
      /*writeMask=*/0xFF,
      reference,
  };
}

VkStencilOpState GetStencilWriteOpState(uint32_t reference) {
  return VkStencilOpState{
      /*failOp=*/VK_STENCIL_OP_KEEP,
      /*passOp=*/VK_STENCIL_OP_REPLACE,
      /*depthFailOp=*/VK_STENCIL_OP_KEEP,
      /*compareOp=*/VK_COMPARE_OP_ALWAYS,
      /*compareMask=*/0xFF,
      /*writeMask=*/0xFF,
      reference,
  };
}

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

std::vector<VertexBuffer::Attribute> ConvertVertexAttributes(
    absl::Span<const common::VertexAttribute> attributes) {
  std::vector<VertexBuffer::Attribute> converted;
  converted.reserve(attributes.size());
  std::transform(
      attributes.begin(), attributes.end(), std::back_inserter(converted),
      [](common::VertexAttribute attrib) -> VertexBuffer::Attribute {
        return {static_cast<uint32_t>(attrib.offset), ChooseFormat(attrib)};
      });
  return converted;
}

} /* namespace pipeline */
} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
