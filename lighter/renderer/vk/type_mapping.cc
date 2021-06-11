//
//  type_mapping.cc
//
//  Created by Pujun Lun on 12/6/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/type_mapping.h"

#include "lighter/common/util.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk::type {

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

VkAttachmentLoadOp ConvertAttachmentLoadOp(AttachmentLoadOp op) {
  switch (op) {
    case AttachmentLoadOp::kLoad:
      return VK_ATTACHMENT_LOAD_OP_LOAD;
    case AttachmentLoadOp::kClear:
      return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case AttachmentLoadOp::kDontCare:
      return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  }
}

VkAttachmentStoreOp ConvertAttachmentStoreOp(AttachmentStoreOp op) {
  switch (op) {
    case AttachmentStoreOp::kStore:
      return VK_ATTACHMENT_STORE_OP_STORE;
    case AttachmentStoreOp::kDontCare:
      return VK_ATTACHMENT_STORE_OP_DONT_CARE;
  }
}

VkBlendFactor ConvertBlendFactor(BlendFactor factor) {
  switch (factor) {
    case BlendFactor::kZero:
      return VK_BLEND_FACTOR_ZERO;
    case BlendFactor::kOne:
      return VK_BLEND_FACTOR_ONE;
    case BlendFactor::kSrcColor:
      return VK_BLEND_FACTOR_SRC_COLOR;
    case BlendFactor::kOneMinusSrcColor:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case BlendFactor::kDstColor:
      return VK_BLEND_FACTOR_DST_COLOR;
    case BlendFactor::kOneMinusDstColor:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case BlendFactor::kSrcAlpha:
      return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFactor::kOneMinusSrcAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::kDstAlpha:
      return VK_BLEND_FACTOR_DST_ALPHA;
    case BlendFactor::kOneMinusDstAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  }
}

VkBlendOp ConvertBlendOp(BlendOp op) {
  switch (op) {
    case BlendOp::kAdd:
      return VK_BLEND_OP_ADD;
    case BlendOp::kSubtract:
      return VK_BLEND_OP_SUBTRACT;
    case BlendOp::kReverseSubtract:
      return VK_BLEND_OP_REVERSE_SUBTRACT;
    case BlendOp::kMin:
      return VK_BLEND_OP_MIN;
    case BlendOp::kMax:
      return VK_BLEND_OP_MAX;
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

VkShaderStageFlags ConvertShaderStages(shader_stage::ShaderStage stages) {
  using ShaderStage = shader_stage::ShaderStage;
  VkShaderStageFlags flags = nullflag;
  common::util::IncludeIfTrue(stages & ShaderStage::VERTEX,
                              flags, VK_SHADER_STAGE_VERTEX_BIT);
  common::util::IncludeIfTrue(stages & ShaderStage::FRAGMENT,
                              flags, VK_SHADER_STAGE_FRAGMENT_BIT);
  common::util::IncludeIfTrue(stages & ShaderStage::COMPUTE,
                              flags, VK_SHADER_STAGE_COMPUTE_BIT);
  return flags;
}

}  // namespace lighter::renderer::vk::type
