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
  kUNorm8,
  kUNorm8Vec2,
  kUNorm8Vec3,
  kUNorm8Vec4,

  kSFloat16,
  kSFloat16Vec2,
  kSFloat16Vec3,
  kSFloat16Vec4,

  kSFloat32,
  kSFloat32Vec2,
  kSFloat32Vec3,
  kSFloat32Vec4,
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
};

} /* namespace shader_stage */

namespace debug_message {

namespace severity {

enum Severity { INFO, WARNING, ERROR };

} /* namespace severity */

namespace type {

enum Type { GENERAL, PERFORMANCE };

} /* namespace type */

struct Config {
  unsigned int severity;
  unsigned int type;
};

} /* namespace debug_message */

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_TYPE_H */
