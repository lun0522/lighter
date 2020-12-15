//
//  pipeline_util.cc
//
//  Created by Pujun Lun on 12/2/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/pipeline_util.h"

#include <algorithm>

#include "lighter/common/util.h"
#include "third_party/absl/types/span.h"

namespace lighter {
namespace renderer {
namespace pipeline {
namespace {

SampleCount GetFirstSupportedSampleCount(
    absl::Span<const SampleCount> candidates,
    absl::FunctionRef<bool(SampleCount)> is_sample_count_supported) {
  const auto iter = std::find_if(candidates.begin(), candidates.end(),
                                 is_sample_count_supported);
  ASSERT_FALSE(iter == candidates.end(), "Failed to find sample count");
  return *iter;
}

} /* namespace */

SampleCount GetSampleCount(
    MultisamplingMode multisampling_mode,
    absl::FunctionRef<bool(SampleCount)> is_sample_count_supported) {
  switch (multisampling_mode) {
    case MultisamplingMode::kNone:
      return SampleCount::k1;

    case MultisamplingMode::kDecent:
      return GetFirstSupportedSampleCount(
          {SampleCount::k4, SampleCount::k2, SampleCount::k1},
          is_sample_count_supported);

    case MultisamplingMode::kBest:
      return GetFirstSupportedSampleCount(
          {SampleCount::k64, SampleCount::k32, SampleCount::k16,
           SampleCount::k8, SampleCount::k4, SampleCount::k2, SampleCount::k1},
          is_sample_count_supported);
  }
}

GraphicsPipelineDescriptor::ColorBlend GetColorBlend() {
  return {
      /*src_color_blend_factor=*/BlendFactor::kOne,
      /*dst_color_blend_factor=*/BlendFactor::kOne,
      /*color_blend_op=*/BlendOp::kAdd,
      /*src_alpha_blend_factor=*/BlendFactor::kZero,
      /*dst_alpha_blend_factor=*/BlendFactor::kZero,
      /*alpha_blend_op=*/BlendOp::kAdd,
  };
}

GraphicsPipelineDescriptor::ColorBlend GetColorAlphaBlend() {
  return {
      /*src_color_blend_factor=*/BlendFactor::kSrcAlpha,
      /*dst_color_blend_factor=*/BlendFactor::kOneMinusSrcAlpha,
      /*color_blend_op=*/BlendOp::kAdd,
      /*src_alpha_blend_factor=*/BlendFactor::kOne,
      /*dst_alpha_blend_factor=*/BlendFactor::kOneMinusSrcAlpha,
      /*alpha_blend_op=*/BlendOp::kAdd,
  };
}

GraphicsPipelineDescriptor::StencilTestOneFace GetStencilNop() {
  return GetStencilRead(CompareOp::kNeverPass, /*reference=*/0);
}

GraphicsPipelineDescriptor::StencilTestOneFace GetStencilRead(
    CompareOp compare_op, unsigned int reference) {
  return {
      /*stencil_fail_op=*/StencilOp::kKeep,
      /*stencil_and_depth_pass_op=*/StencilOp::kKeep,
      /*stencil_pass_depth_fail_op=*/StencilOp::kKeep,
      compare_op,
      /*compare_mask=*/0xFF,
      /*write_mask=*/0,
      reference,
  };
}

GraphicsPipelineDescriptor::StencilTestOneFace GetStencilWrite(
    unsigned int reference) {
  return {
      /*stencil_fail_op=*/StencilOp::kKeep,
      /*stencil_and_depth_pass_op=*/StencilOp::kReplace,
      /*stencil_pass_depth_fail_op=*/StencilOp::kKeep,
      CompareOp::kAlwaysPass,
      /*compare_mask=*/0,
      /*write_mask=*/0xFF,
      reference,
  };
}

GraphicsPipelineDescriptor::Viewport GetFullFrameViewport(
    const glm::ivec2& frame_size) {
  return {/*origin=*/glm::vec2{0.0f}, /*extent=*/frame_size};
}

GraphicsPipelineDescriptor::Viewport GetViewport(const glm::ivec2& frame_size,
                                                 float aspect_ratio) {
  glm::vec2 effective_size = frame_size;
  if (frame_size.x > frame_size.y * aspect_ratio) {
    effective_size.x = frame_size.y * aspect_ratio;
  } else {
    effective_size.y = frame_size.x / aspect_ratio;
  }
  return {/*origin=*/(glm::vec2{frame_size} - effective_size) / 2.0f,
          /*extent=*/effective_size};
}

GraphicsPipelineDescriptor::Scissor GetFullFrameScissor(
    const glm::ivec2& frame_size) {
  return {/*origin=*/glm::ivec2{0}, /*extent=*/frame_size};
}

} /* namespace pipeline */
} /* namespace renderer */
} /* namespace lighter */
