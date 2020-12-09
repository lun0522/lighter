//
//  type_mapping.cc
//
//  Created by Pujun Lun on 12/6/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/type_mapping.h"

#include "lighter/common/util.h"

namespace lighter {
namespace renderer {
namespace vk {
namespace type {

VkVertexInputRate ConvertVertexInputRate(VertexInputRate rate) {
  switch (rate) {
    case VertexInputRate::kVertex:
      return VK_VERTEX_INPUT_RATE_VERTEX;
    case VertexInputRate::kInstance:
      return VK_VERTEX_INPUT_RATE_INSTANCE;
  }
}

VkFormat ConvertDataFormat(DataFormat format) {
  switch (format) {
    case DataFormat::kSFloat32:
      return VK_FORMAT_R32_SFLOAT;
    case DataFormat::kSFloat32Vec2:
      return VK_FORMAT_R32G32_SFLOAT;
    case DataFormat::kSFloat32Vec3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case DataFormat::kSFloat32Vec4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
  }
}

VkCompareOp ConvertCompareOp(CompareOp op) {
  switch (op) {
    case CompareOp::kNeverPass:
      return VK_COMPARE_OP_NEVER;
    case CompareOp::kLess:
      return VK_COMPARE_OP_LESS;
    case CompareOp::kEqual:
      return VK_COMPARE_OP_EQUAL;
    case CompareOp::kLessEqual:
      return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareOp::kGreater:
      return VK_COMPARE_OP_GREATER;
    case CompareOp::kNotEqual:
      return VK_COMPARE_OP_NOT_EQUAL;
    case CompareOp::kGreaterEqual:
      return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case CompareOp::kAlwaysPass:
      return VK_COMPARE_OP_ALWAYS;
  }
}

VkStencilOp ConvertStencilOp(StencilOp op) {
  switch (op) {
    case StencilOp::kKeep:
      return VK_STENCIL_OP_KEEP;
    case StencilOp::kZero:
      return VK_STENCIL_OP_ZERO;
    case StencilOp::kReplace:
      return VK_STENCIL_OP_REPLACE;
  }
}

VkPrimitiveTopology ConvertPrimitiveTopology(PrimitiveTopology topology) {
  switch (topology) {
    case PrimitiveTopology::kPointList:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case PrimitiveTopology::kLineList:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case PrimitiveTopology::kLineStrip:
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case PrimitiveTopology::kTriangleList:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case PrimitiveTopology::kTriangleStrip:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case PrimitiveTopology::kTriangleFan:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
  }
}

VkShaderStageFlagBits ConvertShaderStage(shader_stage::ShaderStage stage) {
  using ShaderStage = shader_stage::ShaderStage;
  ASSERT_TRUE(common::util::IsPowerOf2(stage),
              "'stage' must only contain one shader stage");
  switch (stage) {
    case ShaderStage::VERTEX:
      return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderStage::FRAGMENT:
      return VK_SHADER_STAGE_FRAGMENT_BIT;
    case ShaderStage::COMPUTE:
      return VK_SHADER_STAGE_COMPUTE_BIT;
  }
}

} /* namespace type */
} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
