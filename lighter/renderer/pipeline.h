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

#include "lighter/renderer/buffer.h"
#include "lighter/renderer/type.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/strings/string_view.h"
#include "third_party/glm/glm.hpp"

namespace lighter {
namespace renderer {

class Pipeline {
 public:
  // This class is neither copyable nor movable.
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  virtual ~Pipeline() = default;
};

struct PipelineDescriptor {
  virtual ~PipelineDescriptor() = default;

  // Name of pipeline.
  std::string pipeline_name;
};

struct GraphicsPipelineDescriptor : public PipelineDescriptor {
 public:
  struct Viewport {
    glm::vec2 origin;
    glm::vec2 extent;
  };

  // Modifiers.
  GraphicsPipelineDescriptor& SetName(absl::string_view name) {
    pipeline_name = std::string{name};
    return *this;
  }
  GraphicsPipelineDescriptor& SetShader(shader_stage::ShaderStage stage,
                                        absl::string_view shader_path) {
    shader_path_map.insert({stage, std::string{shader_path}});
    return *this;
  }
  GraphicsPipelineDescriptor& AddVertexInput(VertexBufferView&& buffer_view) {
    vertex_buffer_views.push_back(std::move(buffer_view));
    return *this;
  }
  GraphicsPipelineDescriptor& EnableColorBlend() {
    enable_color_blend = true;
    return *this;
  }
  GraphicsPipelineDescriptor& SetViewport(const Viewport& viewport_info) {
    viewport = viewport_info;
    return *this;
  }

  // Paths to shaders used at each stage.
  absl::flat_hash_map<shader_stage::ShaderStage, std::string> shader_path_map;

  // Vertex input binding and attributes.
  std::vector<VertexBufferView> vertex_buffer_views;

  // Whether to enable color blending (off by default).
  bool enable_color_blend = false;

  // Viewport info.
  Viewport viewport;
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
