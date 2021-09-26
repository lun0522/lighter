//
//  pipeline.h
//
//  Created by Pujun Lun on 12/5/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_PIPELINE_H
#define LIGHTER_RENDERER_VK_PIPELINE_H

#include <memory>
#include <string>
#include <string_view>

#include "lighter/common/ref_count.h"
#include "lighter/common/util.h"
#include "lighter/renderer/ir/pass.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer::vk {

// This class loads a shader from 'file_path' and creates a VkShaderModule.
// Shader modules can be released after the pipeline is built in order to save
// the host memory.
class ShaderModule : public WithSharedContext {
 public:
  // Reference counted shader modules.
  using RefCountedShaderModule = common::RefCountedObject<ShaderModule>;

  // An instance of this will preserve all shader modules created within its
  // surrounding scope, and release them once all AutoReleaseShaderPool objects
  // go out of scope.
  using AutoReleaseShaderPool = RefCountedShaderModule::AutoReleasePool;

  ShaderModule(const SharedContext& context, std::string_view file_path);

  // This class is neither copyable nor movable.
  ShaderModule(const ShaderModule&) = delete;
  ShaderModule& operator=(const ShaderModule&) = delete;

  ~ShaderModule() {
    context_->DeviceDestroy(shader_module_);
  }

  // Overloads.
  intl::ShaderModule operator*() const { return shader_module_; }

 private:
  // Opaque shader module object.
  intl::ShaderModule shader_module_;
};

class Pipeline : public WithSharedContext {
 public:
  // Constructs a graphics pipeline.
  Pipeline(const SharedContext& context,
           const ir::GraphicsPipelineDescriptor& descriptor,
           intl::SampleCountFlagBits sample_count,
           intl::RenderPass render_pass, int subpass_index);

  // Constructs a compute pipeline.
  Pipeline(const SharedContext& context,
           const ir::ComputePipelineDescriptor& descriptor);

  // This class is neither copyable nor movable.
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  ~Pipeline();

  // Binds to this pipeline. This should be called when 'command_buffer' is
  // recording commands.
  void Bind(intl::CommandBuffer command_buffer) const;

 private:
  Pipeline(const SharedContext& context, std::string_view name,
           intl::PipelineBindPoint binding_point,
           const ir::PipelineDescriptor::UniformDescriptor& uniform_descriptor);

  // Name of pipeline.
  const std::string name_;

  // Pipeline binding point, either graphics or compute.
  const intl::PipelineBindPoint binding_point_;

  // Opaque pipeline layout object.
  intl::PipelineLayout pipeline_layout_;

  // Opaque pipeline object.
  intl::Pipeline pipeline_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_PIPELINE_H
