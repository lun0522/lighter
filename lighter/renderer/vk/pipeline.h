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
#include "lighter/renderer/pass.h"
#include "lighter/renderer/pipeline.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/image.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter::renderer::vk {

// This class loads a shader from 'file_path' and creates a VkShaderModule.
// Shader modules can be released after the pipeline is built in order to save
// the host memory.
class ShaderModule {
 public:
  // Reference counted shader modules.
  using RefCountedShaderModule = common::RefCountedObject<ShaderModule>;

  // An instance of this will preserve all shader modules created within its
  // surrounding scope, and release them once all AutoReleaseShaderPool objects
  // go out of scope.
  using AutoReleaseShaderPool = RefCountedShaderModule::AutoReleasePool;

  ShaderModule(SharedContext context, std::string_view file_path);

  // This class is neither copyable nor movable.
  ShaderModule(const ShaderModule&) = delete;
  ShaderModule& operator=(const ShaderModule&) = delete;

  ~ShaderModule() {
    vkDestroyShaderModule(*context_->device(), shader_module_,
                          *context_->host_allocator());
  }

  // Overloads.
  const VkShaderModule& operator*() const { return shader_module_; }

 private:
  // Pointer to context.
  const SharedContext context_;

  // Opaque shader module object.
  VkShaderModule shader_module_ = VK_NULL_HANDLE;
};

class Pipeline {
 public:
  // Constructs a graphics pipeline.
  Pipeline(SharedContext context, const GraphicsPipelineDescriptor& descriptor,
           const VkRenderPass& render_pass, int subpass_index,
           absl::Span<const DeviceImage* const> subpass_attachments);

  // Constructs a compute pipeline.
  Pipeline(SharedContext context, const ComputePipelineDescriptor& descriptor);

  // This class is neither copyable nor movable.
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  ~Pipeline();

  // Binds to this pipeline. This should be called when 'command_buffer' is
  // recording commands.
  void Bind(const VkCommandBuffer& command_buffer) const;

 private:
  Pipeline(SharedContext context, std::string_view name,
           VkPipelineBindPoint binding_point,
           const PipelineDescriptor::UniformDescriptor& uniform_descriptor);

  // Pointer to context.
  const SharedContext context_;

  // Name of pipeline.
  const std::string name_;

  // Pipeline binding point, either graphics or compute.
  const VkPipelineBindPoint binding_point_;

  // Opaque pipeline layout object.
  VkPipelineLayout layout_ = VK_NULL_HANDLE;

  // Opaque pipeline object.
  VkPipeline pipeline_ = VK_NULL_HANDLE;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_PIPELINE_H
