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

  virtual ~Pipeline() = default;

  // Binds to this pipeline.
  virtual void Bind() const = 0;
};

class GraphicsPipelineDescriptor {
 public:
  // This class provides copy constructor and move constructor.
  GraphicsPipelineDescriptor(GraphicsPipelineDescriptor&&) noexcept = default;
  GraphicsPipelineDescriptor(const GraphicsPipelineDescriptor&) = default;

  virtual ~GraphicsPipelineDescriptor() = default;

  // Sets a name for this pipeline. This is for debugging purpose.
  virtual GraphicsPipelineDescriptor& SetName(absl::string_view name) = 0;

  // Sets shader for 'stage'.
  virtual GraphicsPipelineDescriptor& SetShader(
      shader_stage::ShaderStage stage, absl::string_view file_path) = 0;

  // Binds input vertex data.
  virtual GraphicsPipelineDescriptor& AddVertexInput(
      const VertexBufferView& buffer_view) = 0;
};

class ComputePipelineDescriptor {
 public:
  // This class provides copy constructor and move constructor.
  ComputePipelineDescriptor(ComputePipelineDescriptor&&) noexcept = default;
  ComputePipelineDescriptor(const ComputePipelineDescriptor&) = default;

  virtual ~ComputePipelineDescriptor() = default;

  // Sets a name for this pipeline. This is for debugging purpose.
  virtual ComputePipelineDescriptor& SetName(absl::string_view name) = 0;

  // Sets compute shader.
  virtual ComputePipelineDescriptor& SetShader(absl::string_view file_path) = 0;
};

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_PIPELINE_H */
