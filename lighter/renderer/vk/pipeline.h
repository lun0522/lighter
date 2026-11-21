//
//  pipeline.h
//
//  Created by Pujun Lun on 12/5/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_PIPELINE_H
#define LIGHTER_RENDERER_VK_PIPELINE_H

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "lighter/base/ref_count.h"
#include "lighter/base/util.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/vk/buffer.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/image.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk {

// Reflected descriptor binding extracted from SPIR-V.
struct ReflectedBinding {
  uint32_t set = 0;
  uint32_t binding = 0;
  intl::DescriptorType descriptor_type = intl::DescriptorType::eUniformBuffer;
  uint32_t descriptor_count = 1;
  intl::ShaderStageFlags stage_flags;
  std::string name;
};

// Reflected push constant range extracted from SPIR-V.
struct ReflectedPushConstantRange {
  intl::ShaderStageFlags stage_flags;
  uint32_t offset = 0;
  uint32_t size = 0;
};

// Combined descriptor set and push constant metadata for one shader or
// pipeline.
struct PipelineReflection {
  std::vector<ReflectedBinding> bindings;
  std::vector<ReflectedPushConstantRange> push_constant_ranges;
};

// Reflects descriptor bindings and push constant ranges from compiled SPIR-V
// words using SPIRV-Cross.
PipelineReflection ReflectSpirv(std::span<const uint32_t> spirv_words,
                                intl::ShaderStageFlagBits* out_stage = nullptr);

// Merges per-stage reflections across a pipeline, combining stageFlags for
// shared (set, binding) entries and validating layout compatibility.
PipelineReflection MergeReflections(
    std::span<const PipelineReflection> stage_reflections);

// This class loads a shader from 'file_path', reflects its SPIR-V interface,
// and creates a VkShaderModule. Shader modules can be released after the
// pipeline is built in order to save host memory.
class ShaderModule : public WithSharedContext {
 public:
  // Reference counted shader modules.
  using RefCountedShaderModule = base::RefCountedObject<ShaderModule>;

  // An instance of this will preserve all shader modules created within its
  // surrounding scope, and release them once all AutoReleaseShaderPool objects
  // go out of scope.
  using AutoReleaseShaderPool = RefCountedShaderModule::AutoReleasePool;

  ShaderModule(const SharedContext& context, std::string_view file_path);

  // This class is neither copyable nor movable.
  ShaderModule(const ShaderModule&) = delete;
  ShaderModule& operator=(const ShaderModule&) = delete;

  ~ShaderModule() { context_->DeviceDestroy(shader_module_); }

  // Accessors.
  intl::ShaderStageFlagBits stage() const { return stage_; }
  const PipelineReflection& reflection() const { return reflection_; }

  // Overloads.
  intl::ShaderModule operator*() const { return shader_module_; }

 private:
  intl::ShaderStageFlagBits stage_ = intl::ShaderStageFlagBits::eVertex;
  PipelineReflection reflection_;

  // Opaque shader module object.
  intl::ShaderModule shader_module_;
};

// Wraps a Vulkan graphics or compute pipeline, descriptor set layouts, and
// pipeline layout built via SPIR-V reflection.
class Pipeline : public WithSharedContext {
 public:
  // Attachment formats used for Dynamic Rendering when building a graphics
  // pipeline without a VkRenderPass.
  struct RenderingInfo {
    std::vector<intl::Format> color_attachment_formats;
    intl::Format depth_attachment_format = intl::Format::eUndefined;
    intl::Format stencil_attachment_format = intl::Format::eUndefined;
  };

  // Describes a buffer resource to bind via push descriptors.
  struct BufferBinding {
    uint32_t binding = 0;
    const Buffer* buffer = nullptr;
    intl::DeviceSize offset = 0;
    intl::DeviceSize range = VK_WHOLE_SIZE;
    uint32_t array_element = 0;
  };

  // Describes an image (and optional sampler) to bind via push descriptors.
  struct ImageBinding {
    uint32_t binding = 0;
    const Image* image = nullptr;
    const Sampler* sampler = nullptr;
    intl::ImageLayout layout = intl::ImageLayout::eShaderReadOnlyOptimal;
    uint32_t array_element = 0;
  };

  // Constructs a graphics pipeline for Dynamic Rendering.
  Pipeline(const SharedContext& context,
           const ir::GraphicsPipelineDescriptor& descriptor,
           intl::SampleCountFlagBits sample_count,
           const RenderingInfo& rendering_info);

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

  // Updates descriptor bindings for 'set' using VK_KHR_push_descriptor.
  void PushDescriptors(intl::CommandBuffer command_buffer,
                       std::span<const BufferBinding> buffer_bindings,
                       std::span<const ImageBinding> image_bindings = {},
                       uint32_t set = 0) const;

  // Uploads push constant bytes to 'command_buffer' for 'stage_flags'.
  void PushConstants(intl::CommandBuffer command_buffer,
                     intl::ShaderStageFlags stage_flags, uint32_t offset,
                     uint32_t size, const void* data) const;

  template <typename T>
  void PushConstant(intl::CommandBuffer command_buffer,
                    intl::ShaderStageFlags stage_flags, uint32_t offset,
                    const T& value) const {
    PushConstants(command_buffer, stage_flags, offset, sizeof(T), &value);
  }

  // Accessors.
  const PipelineReflection& reflection() const { return reflection_; }
  intl::PipelineLayout layout() const { return pipeline_layout_; }

 private:
  void InitPipelineLayout(
      std::span<const PipelineReflection> stage_reflections,
      const ir::PipelineDescriptor::UniformDescriptor& uniform_descriptor);

  // Name of pipeline.
  const std::string name_;

  // Pipeline binding point, either graphics or compute.
  const intl::PipelineBindPoint binding_point_;

  PipelineReflection reflection_;
  std::vector<intl::DescriptorSetLayout> descriptor_set_layouts_;

  // Opaque pipeline layout object.
  intl::PipelineLayout pipeline_layout_;

  // Opaque pipeline object.
  intl::Pipeline pipeline_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_PIPELINE_H
