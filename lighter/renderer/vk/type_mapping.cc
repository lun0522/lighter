//
//  type_mapping.cc
//
//  Created by Pujun Lun on 12/6/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/type_mapping.h"

#include "lighter/common/util.h"

namespace lighter::renderer::vk::type {

using namespace ir;

intl::VertexInputRate ConvertVertexInputRate(VertexInputRate rate) {
  #define CONVERT(rate) \
      case VertexInputRate::k##rate: return intl::VertexInputRate::e##rate
  switch (rate) {
    CONVERT(Vertex);
    CONVERT(Instance);
  }
  #undef CONVERT
}

intl::Format ConvertDataFormat(DataFormat format) {
  #define CONVERT_ALIAS(format, format_intl) \
      case DataFormat::k##format: return intl::Format::e##format_intl
  switch (format) {
    CONVERT_ALIAS(SFloat32, R32Sfloat);
    CONVERT_ALIAS(SFloat32Vec2, R32G32Sfloat);
    CONVERT_ALIAS(SFloat32Vec3, R32G32B32Sfloat);
    CONVERT_ALIAS(SFloat32Vec4, R32G32B32A32Sfloat);
  }
  #undef CONVERT_ALIAS
}

intl::AttachmentLoadOp ConvertAttachmentLoadOp(AttachmentLoadOp op) {
  #define CONVERT(op) \
      case AttachmentLoadOp::k##op: return intl::AttachmentLoadOp::e##op
  switch (op) {
    CONVERT(Load);
    CONVERT(Clear);
    CONVERT(DontCare);
  }
  #undef CONVERT
}

intl::AttachmentStoreOp ConvertAttachmentStoreOp(AttachmentStoreOp op) {
  #define CONVERT(op) \
      case AttachmentStoreOp::k##op: return intl::AttachmentStoreOp::e##op
  switch (op) {
    CONVERT(Store);
    CONVERT(DontCare);
  }
  #undef CONVERT
}

intl::BlendFactor ConvertBlendFactor(BlendFactor factor) {
  #define CONVERT(factor) \
      case BlendFactor::k##factor: return intl::BlendFactor::e##factor
  switch (factor) {
    CONVERT(Zero);
    CONVERT(One);
    CONVERT(SrcColor);
    CONVERT(OneMinusSrcColor);
    CONVERT(DstColor);
    CONVERT(OneMinusDstColor);
    CONVERT(SrcAlpha);
    CONVERT(OneMinusSrcAlpha);
    CONVERT(DstAlpha);
    CONVERT(OneMinusDstAlpha);
  }
  #undef CONVERT
}

intl::BlendOp ConvertBlendOp(BlendOp op) {
  #define CONVERT(op) case BlendOp::k##op: return intl::BlendOp::e##op
  switch (op) {
    CONVERT(Add);
    CONVERT(Subtract);
    CONVERT(ReverseSubtract);
    CONVERT(Min);
    CONVERT(Max);
  }
  #undef CONVERT
}

intl::CompareOp ConvertCompareOp(CompareOp op) {
  #define CONVERT(op) case CompareOp::k##op: return intl::CompareOp::e##op
  #define CONVERT_ALIAS(op, op_intl) \
      case CompareOp::k##op: return intl::CompareOp::e##op_intl
  switch (op) {
    CONVERT_ALIAS(NeverPass, Never);
    CONVERT(Less);
    CONVERT(Equal);
    CONVERT_ALIAS(LessEqual, LessOrEqual);
    CONVERT(Greater);
    CONVERT(NotEqual);
    CONVERT_ALIAS(GreaterEqual, GreaterOrEqual);
    CONVERT_ALIAS(AlwaysPass, Always);
  }
  #undef CONVERT
  #undef CONVERT_ALIAS
}

intl::StencilOp ConvertStencilOp(StencilOp op) {
  #define CONVERT(op) case StencilOp::k##op: return intl::StencilOp::e##op
  switch (op) {
    CONVERT(Keep);
    CONVERT(Zero);
    CONVERT(Replace);
  }
  #undef CONVERT
}

intl::PrimitiveTopology ConvertPrimitiveTopology(PrimitiveTopology topology) {
  #define CONVERT(topology)                 \
      case PrimitiveTopology::k##topology:  \
        return intl::PrimitiveTopology::e##topology
  switch (topology) {
    CONVERT(PointList);
    CONVERT(LineList);
    CONVERT(LineStrip);
    CONVERT(TriangleList);
    CONVERT(TriangleStrip);
    CONVERT(TriangleFan);
  }
  #undef CONVERT
}

intl::ShaderStageFlagBits ConvertShaderStage(shader_stage::ShaderStage stage) {
  ASSERT_TRUE(common::util::IsPowerOf2(stage),
              "'stage' must only contain one shader stage");
  #define CONVERT_ALIAS(stage, stage_intl)    \
      case shader_stage::ShaderStage::stage:  \
          return intl::ShaderStageFlagBits::e##stage_intl
  switch (stage) {
    CONVERT_ALIAS(VERTEX, Vertex);
    CONVERT_ALIAS(FRAGMENT, Fragment);
    CONVERT_ALIAS(COMPUTE, Compute);
  }
  #undef CONVERT_ALIAS
}

intl::ShaderStageFlags ConvertShaderStages(shader_stage::ShaderStage stages) {
  intl::ShaderStageFlags stages_intl;
  #define INCLUDE_STAGE(stage, stage_intl)            \
      common::util::IncludeIfTrue(                    \
          stages & shader_stage::ShaderStage::stage,  \
          stages_intl, intl::ShaderStageFlagBits::e##stage_intl)
  INCLUDE_STAGE(VERTEX, Vertex);
  INCLUDE_STAGE(FRAGMENT, Fragment);
  INCLUDE_STAGE(COMPUTE, Compute);
  #undef INCLUDE_STAGE
  return stages_intl;
}

intl::DebugUtilsMessageSeverityFlagsEXT ConvertDebugMessageSeverities(
    uint32_t severities) {
  intl::DebugUtilsMessageSeverityFlagsEXT severities_intl;
  #define INCLUDE_SEVERITY(severity_enum, severity_intl)        \
      common::util::IncludeIfTrue(                              \
          severities & debug_message::severity::severity_enum,  \
          severities_intl,                                      \
          intl::DebugUtilsMessageSeverityFlagBitsEXT::e##severity_intl)
  INCLUDE_SEVERITY(VERBOSE, Verbose);
  INCLUDE_SEVERITY(INFO, Info);
  INCLUDE_SEVERITY(WARNING, Warning);
  INCLUDE_SEVERITY(ERROR, Error);
  #undef INCLUDE_SEVERITY
  return severities_intl;
}

intl::DebugUtilsMessageTypeFlagsEXT ConvertDebugMessageTypes(uint32_t types) {
  intl::DebugUtilsMessageTypeFlagsEXT types_intl;
  #define INCLUDE_TYPE(type_enum, type_intl)                  \
      common::util::IncludeIfTrue(                            \
          types & debug_message::type::type_enum, types_intl, \
          intl::DebugUtilsMessageTypeFlagBitsEXT::e##type_intl)
  INCLUDE_TYPE(GENERAL, General);
  INCLUDE_TYPE(GENERAL, Validation);
  INCLUDE_TYPE(PERFORMANCE, Performance);
  #undef INCLUDE_TYPE
  return types_intl;
}

}  // namespace lighter::renderer::vk::type
