//
//  type.h
//
//  Created by Pujun Lun on 10/13/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_TYPE_H
#define LIGHTER_RENDERER_TYPE_H

namespace lighter {
namespace renderer {

enum class DataFormat {
  kSFloat32,
  kSFloat32Vec2,
  kSFloat32Vec3,
  kSFloat32Vec4,
};

// Specifies the rate at which vertex attributes are pulled from the buffer.
enum class VertexInputRate { kVertex, kInstance };

// Whether to read and/or write.
enum class AccessType {
  kDontCare,
  kReadOnly,
  kWriteOnly,
  kReadWrite,
};

// Where to access a buffer or image. Note that kOther is different from
// kDontCare. For example, depth stencil attachments are actually not written in
// fragment shader. It has its own pipeline stages.
enum class AccessLocation {
  kDontCare,
  kHost,
  kVertexShader,
  kFragmentShader,
  kComputeShader,
  kOther,
};

enum class AttachmentLoadOp { kLoad, kClear, kDontCare };

enum class AttachmentStoreOp { kStore, kDontCare };

enum class FilterType { kNearest, kLinear };

enum class SamplerAddressMode {
  kRepeat,
  kMirroredRepeat,
  kClampToEdge,
  kClampToBorder,
  kMirrorClamToEdge,
};

enum class MultisamplingMode { kNone, kDecent, kBest };

enum class SampleCount { k1, k2, k4, k8, k16, k32, k64 };

enum class BlendFactor {
  kZero,
  kOne,
  kSrcColor,
  kOneMinusSrcColor,
  kDstColor,
  kOneMinusDstColor,
  kSrcAlpha,
  kOneMinusSrcAlpha,
  kDstAlpha,
  kOneMinusDstAlpha,
};

enum class BlendOp {
  kAdd,
  kSubtract,
  kReverseSubtract,
  kMin,
  kMax,
};

enum class CompareOp {
  kNeverPass,
  kLess,
  kEqual,
  kLessEqual,
  kGreater,
  kNotEqual,
  kGreaterEqual,
  kAlwaysPass,
};

enum class StencilOp {
  kKeep,
  kZero,
  kReplace,
};

enum class PrimitiveTopology {
  kPointList,
  kLineList,
  kLineStrip,
  kTriangleList,
  kTriangleStrip,
  kTriangleFan,
};

namespace shader_stage {

enum ShaderStage {
  VERTEX   = 1U << 0U,
  FRAGMENT = 1U << 1U,
  COMPUTE  = 1U << 2U,
};

} /* namespace shader_stage */

namespace debug_message {

namespace severity {

enum Severity {
  INFO    = 1U << 0U,
  WARNING = 1U << 1U,
  ERROR   = 1U << 2U,
};

} /* namespace severity */

namespace type {

enum Type {
  GENERAL     = 1U << 0U,
  PERFORMANCE = 1U << 1U,
};

} /* namespace type */

struct Config {
  unsigned int message_severity;
  unsigned int message_type;
};

} /* namespace debug_message */

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_TYPE_H */
