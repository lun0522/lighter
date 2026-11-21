//
//  pipeline.h
//
//  Created by Pujun Lun on 10/14/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IR_PIPELINE_H
#define LIGHTER_RENDERER_IR_PIPELINE_H

#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "lighter/base/util.h"
#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/type.h"

namespace lighter::renderer::ir {

// Base descriptor shared by graphics and compute pipelines.
struct PipelineDescriptor {
  // Shader-visible push constant range covering one or more stages.
  struct PushConstantRange {
    shader_stage::ShaderStage shader_stages;
    int offset;
    int size;
  };

  // Shader uniform and push constant layout referenced by the pipeline.
  struct UniformDescriptor {
    std::vector<PushConstantRange> push_constant_ranges;
  };

  virtual ~PipelineDescriptor() = default;

  void AddPushConstantRangeBase(const PushConstantRange& range) {
    uniform_descriptor.push_constant_ranges.push_back(range);
  }

  // Name of pipeline.
  std::string pipeline_name;

  UniformDescriptor uniform_descriptor;
};

// Declarative state and shader configuration for a graphics pipeline.
struct GraphicsPipelineDescriptor : public PipelineDescriptor {
 public:
  // Paths to shaders used at each stage.
  using ShaderPathMap =
      std::unordered_map<shader_stage::ShaderStage, std::string>;

  // Color and alpha blending equations for a single color attachment.
  struct ColorBlend {
    BlendFactor src_color_blend_factor;
    BlendFactor dst_color_blend_factor;
    BlendOp color_blend_op;
    BlendFactor src_alpha_blend_factor;
    BlendFactor dst_alpha_blend_factor;
    BlendOp alpha_blend_op;
  };

  // Depth comparison and depth-write settings for rasterization.
  struct DepthTest {
    bool enable_test;
    bool enable_write;
    CompareOp compare_op;
  };

  // Stencil test operations and masks configured for either front or back
  // faces.
  struct StencilTestOneFace {
    StencilOp stencil_fail_op;
    StencilOp stencil_and_depth_pass_op;
    StencilOp stencil_pass_depth_fail_op;
    CompareOp compare_op;
    unsigned int compare_mask;
    unsigned int write_mask;
    unsigned int reference;
  };

  // Dual-sided stencil test configuration for front and back polygon faces.
  struct StencilTest {
    enum FaceIndex {
      kFrontFaceIndex = 0,
      kBackFaceIndex,
      kNumFaces,
    };

    bool enable_test;
    StencilTestOneFace tests[kNumFaces];
  };

  // Normalized or pixel-space viewport rectangle.
  struct Viewport {
    glm::vec2 origin;
    glm::vec2 extent;
  };

  // Pixel-space scissor clipping rectangle.
  struct Scissor {
    glm::ivec2 origin;
    glm::ivec2 extent;
  };

  // Combined viewport/scissor state with optional Y-axis inversion for
  // Vulkan/OpenGL parity.
  struct ViewportConfig {
    Viewport viewport;
    Scissor scissor;
    bool flip_y = true;
  };

  // Modifiers.
  GraphicsPipelineDescriptor& SetName(std::string_view name) {
    pipeline_name = std::string{name};
    return *this;
  }
  GraphicsPipelineDescriptor& SetShader(shader_stage::ShaderStage stage,
                                        std::string_view shader_path) {
    ASSERT_TRUE(base::util::IsPowerOf2(stage),
                "Exactly one shader stage is allowed");
    shader_path_map.insert({stage, std::string{shader_path}});
    return *this;
  }
  GraphicsPipelineDescriptor& UseColorAttachment(
      int location, const std::optional<ColorBlend>& color_blend) {
    color_attachment_map.insert({location, color_blend});
    return *this;
  }
  GraphicsPipelineDescriptor& AddVertexInput(VertexBufferView&& buffer_view) {
    vertex_buffer_views.push_back(std::move(buffer_view));
    return *this;
  }
  GraphicsPipelineDescriptor& AddPushConstantRange(
      const PushConstantRange& range) {
    AddPushConstantRangeBase(range);
    return *this;
  }
  GraphicsPipelineDescriptor& EnableDepthTestOnly(
      CompareOp compare_op = CompareOp::kLessEqual) {
    depth_test = {/*enable_test=*/true, /*enable_write=*/false, compare_op};
    return *this;
  }
  GraphicsPipelineDescriptor& EnableDepthTestAndWrite(
      CompareOp compare_op = CompareOp::kLessEqual) {
    depth_test = {/*enable_test=*/true, /*enable_write=*/true, compare_op};
    return *this;
  }
  GraphicsPipelineDescriptor& EnableStencilTest(
      const StencilTestOneFace& front_face_test,
      const StencilTestOneFace& back_face_test) {
    stencil_test.enable_test = true;
    stencil_test.tests[StencilTest::kFrontFaceIndex] = front_face_test;
    stencil_test.tests[StencilTest::kBackFaceIndex] = back_face_test;
    return *this;
  }
  GraphicsPipelineDescriptor& SetViewport(const Viewport& viewport,
                                          const Scissor& scissor,
                                          bool flip_y = true) {
    viewport_config = {viewport, scissor, flip_y};
    return *this;
  }
  GraphicsPipelineDescriptor& SetPrimitiveTopology(PrimitiveTopology topology) {
    primitive_topology = topology;
    return *this;
  }

  std::unordered_map<shader_stage::ShaderStage, std::string> shader_path_map;
  std::unordered_map<int, std::optional<ColorBlend>> color_attachment_map;
  std::vector<VertexBufferView> vertex_buffer_views;
  DepthTest depth_test;
  StencilTest stencil_test;
  ViewportConfig viewport_config;
  PrimitiveTopology primitive_topology = PrimitiveTopology::kTriangleList;
};

// Declarative configuration for a compute shader pipeline.
class ComputePipelineDescriptor : public PipelineDescriptor {
 public:
  // Modifiers.
  ComputePipelineDescriptor& SetName(std::string_view name) {
    pipeline_name = std::string{name};
    return *this;
  }
  ComputePipelineDescriptor& SetShader(std::string_view path) {
    shader_path = std::string{path};
    return *this;
  }

  // Path to compute shader.
  std::string shader_path;
};

}  // namespace lighter::renderer::ir

#endif  // LIGHTER_RENDERER_IR_PIPELINE_H
