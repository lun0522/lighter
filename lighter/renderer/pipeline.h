//
//  pipeline.h
//
//  Created by Pujun Lun on 10/14/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_PIPELINE_H
#define LIGHTER_RENDERER_PIPELINE_H

#include <memory>
#include <string>

#include "lighter/common/util.h"
#include "lighter/renderer/buffer.h"
#include "lighter/renderer/type.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/strings/string_view.h"
#include "third_party/glm/glm.hpp"

namespace lighter {
namespace renderer {

class Pipeline {
 public:
  Pipeline(absl::string_view name) : name_{name} {}

  // This class is neither copyable nor movable.
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  virtual ~Pipeline() = default;

 protected:
  // Accessors.
  const std::string& name() const { return name_; }

 private:
  // Name of pipeline.
  const std::string name_;
};

struct PipelineDescriptor {
  virtual ~PipelineDescriptor() = default;

  // Name of pipeline.
  std::string pipeline_name;
};

struct GraphicsPipelineDescriptor : public PipelineDescriptor {
 public:
  // Paths to shaders used at each stage.
  using ShaderPathMap = absl::flat_hash_map<shader_stage::ShaderStage,
                                            std::string>;

  struct DepthTest {
    bool enable_test;
    bool enable_write;
    CompareOp compare_op;
  };

  struct StencilTestOneFace {
    StencilOp stencil_fail_op;
    StencilOp stencil_and_depth_pass_op;
    StencilOp stencil_pass_depth_fail_op;
    CompareOp compare_op;
    unsigned int compare_mask;
    unsigned int write_mask;
    unsigned int reference;
  };

  struct StencilTest {
    enum FaceIndex {
      kFrontFaceIndex = 0,
      kBackFaceIndex,
      kNumFaces,
    };

    bool enable_test;
    StencilTestOneFace tests[kNumFaces];
  };

  struct Viewport {
    glm::vec2 origin;
    glm::vec2 extent;
  };

  struct Scissor {
    glm::ivec2 origin;
    glm::ivec2 extent;
  };

  struct ViewportConfig {
    Viewport viewport;
    Scissor scissor;
    bool flip_y;
  };

  // Modifiers.
  GraphicsPipelineDescriptor& SetName(absl::string_view name) {
    pipeline_name = std::string{name};
    return *this;
  }
  GraphicsPipelineDescriptor& SetShader(shader_stage::ShaderStage stage,
                                        absl::string_view shader_path) {
    ASSERT_TRUE(common::util::IsPowerOf2(stage),
                "Exactly one shader stage is allowed");
    shader_path_map.insert({stage, std::string{shader_path}});
    return *this;
  }
  GraphicsPipelineDescriptor& AddVertexInput(VertexBufferView&& buffer_view) {
    vertex_buffer_views.push_back(std::move(buffer_view));
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

  absl::flat_hash_map<shader_stage::ShaderStage, std::string> shader_path_map;
  std::vector<VertexBufferView> vertex_buffer_views;
  DepthTest depth_test;
  StencilTest stencil_test;
  ViewportConfig viewport_config;
  PrimitiveTopology primitive_topology = PrimitiveTopology::kTriangleList;
};

class ComputePipelineDescriptor : public PipelineDescriptor {
 public:
  // Modifiers.
  ComputePipelineDescriptor& SetName(absl::string_view name) {
    pipeline_name = std::string{name};
    return *this;
  }
  ComputePipelineDescriptor& SetShader(absl::string_view path) {
    shader_path = std::string{path};
    return *this;
  }

  // Path to compute shader.
  std::string shader_path;
};

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_PIPELINE_H */
