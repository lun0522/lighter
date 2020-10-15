//
//  pipeline.h
//
//  Created by Pujun Lun on 10/14/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_PIPELINE_H
#define LIGHTER_RENDERER_PIPELINE_H

#include <memory>

#include "lighter/renderer/buffer.h"
#include "lighter/renderer/type.h"
#include "third_party/absl/strings/string_view.h"

namespace lighter {
namespace renderer {

class Pipeline {
 public:
  // This class is neither copyable nor movable.
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  ~Pipeline() = default;

  // Binds to this pipeline.
  virtual void Bind() const = 0;
};

class PipelineBuilder {
 public:
  // This class provides copy constructor and move constructor.
  PipelineBuilder(PipelineBuilder&&) noexcept = default;
  PipelineBuilder(const PipelineBuilder&) = default;

  ~PipelineBuilder() = default;

  // Builds a pipeline. Internal states are preserved, so that the builder can
  // be modified and used to build another pipeline later.
  virtual std::unique_ptr<Pipeline> Build() const = 0;
};

class GraphicsPipelineBuilder {
 public:
  // This class provides copy constructor and move constructor.
  GraphicsPipelineBuilder(GraphicsPipelineBuilder&&) noexcept = default;
  GraphicsPipelineBuilder(const GraphicsPipelineBuilder&) = default;

  // Sets a name for this pipeline. This is for debugging purpose.
  virtual GraphicsPipelineBuilder& SetName(absl::string_view name) = 0;

  // Sets shader for 'stage'.
  virtual GraphicsPipelineBuilder& SetShader(shader_stage::ShaderStage stage,
                                             absl::string_view file_path) = 0;

  // Binds input vertex data.
  virtual GraphicsPipelineBuilder& AddVertexInput(
      const VertexBufferView& buffer_view) = 0;
};

class ComputePipelineBuilder {
 public:
  // This class provides copy constructor and move constructor.
  ComputePipelineBuilder(ComputePipelineBuilder&&) noexcept = default;
  ComputePipelineBuilder(const ComputePipelineBuilder&) = default;

  // Sets a name for this pipeline. This is for debugging purpose.
  virtual ComputePipelineBuilder& SetName(absl::string_view name) = 0;

  // Sets compute shader.
  virtual ComputePipelineBuilder& SetShader(absl::string_view file_path) = 0;
};

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_PIPELINE_H */
