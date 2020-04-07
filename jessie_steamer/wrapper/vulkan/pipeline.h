//
//  pipeline.h
//
//  Created by Pujun Lun on 2/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_H

#include <memory>
#include <string>
#include <vector>

#include "jessie_steamer/common/ref_count.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/optional.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// Forward declarations.
class Pipeline;

// This class loads a shader from 'file_path' and creates a VkShaderModule.
// Shader modules can be released after the pipeline is built in order to save
// the host memory. The user can avoid this happening by instantiating an
// AutoReleaseShaderPool.
class ShaderModule {
 public:
  // Reference counted shader modules.
  using RefCountedShaderModule = common::RefCountedObject<ShaderModule>;

  // An instance of this will preserve all shader modules created within its
  // surrounding scope, and release them once all AutoReleaseShaderPool objects
  // go out of scope.
  using AutoReleaseShaderPool = RefCountedShaderModule::AutoReleasePool;

  ShaderModule(SharedBasicContext context, const std::string& file_path);

  // This class is neither copyable nor movable.
  ShaderModule(const ShaderModule&) = delete;
  ShaderModule& operator=(const ShaderModule&) = delete;

  ~ShaderModule() {
    vkDestroyShaderModule(*context_->device(), shader_module_,
                          *context_->allocator());
  }

  // Overloads.
  const VkShaderModule& operator*() const { return shader_module_; }

 private:
  // Pointer to context.
  const SharedBasicContext context_;

  // Opaque shader module object.
  VkShaderModule shader_module_;
};

// This is the base class of all pipeline builder classes. The user should use
// it through derived classes.
class PipelineBuilder {
 public:
  // This class is neither copyable nor movable.
  PipelineBuilder(const PipelineBuilder&) = delete;
  PipelineBuilder& operator=(const PipelineBuilder&) = delete;

  virtual ~PipelineBuilder() = default;

  // Builds a VkPipeline object. This can be called multiple times.
  virtual std::unique_ptr<Pipeline> Build() const = 0;

 protected:
  explicit PipelineBuilder(SharedBasicContext context)
      : context_{std::move(FATAL_IF_NULL(context))} {}

  // Sets the name for the pipeline.
  void SetName(std::string&& name) { name_ = std::move(name); }

  // Sets descriptor set layouts and push constant ranges used in this pipeline.
  void SetLayout(std::vector<VkDescriptorSetLayout>&& descriptor_layouts,
                 std::vector<VkPushConstantRange>&& push_constant_ranges);

  // Accessors.
  SharedBasicContext context() const { return context_; }
  const std::string& name() const { return name_; }
  bool has_pipeline_layout_info() const {
    return pipeline_layout_info_.has_value();
  }
  const VkPipelineLayoutCreateInfo& pipeline_layout_info() const {
    return pipeline_layout_info_.value();
  }

 private:
  // Pointer to context.
  const SharedBasicContext context_;

  // Name of the pipeline (used for debugging).
  std::string name_;

  // Descriptor sets and push constants determine the layout of the pipeline.
  absl::optional<VkPipelineLayoutCreateInfo> pipeline_layout_info_;
  std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  std::vector<VkPushConstantRange> push_constant_ranges_;
};

// The user should use this class to create graphics pipelines. All internal
// states are preserved when it is used to build a pipeline, so it can be reused
// for building another pipeline. Exceptionally, by default, shader modules are
// released after building one pipeline to save the host memory. This behavior
// can be changed by the user. See class comments of ShaderModule.
class GraphicsPipelineBuilder : public PipelineBuilder {
 public:
  // Describes a viewport transformation.
  struct ViewportInfo {
    VkViewport viewport;
    VkRect2D scissor;
  };

  // Internal states will be filled with default settings, unless they are of
  // absl::optional or std::vector types.
  explicit GraphicsPipelineBuilder(SharedBasicContext context);

  // This class is neither copyable nor movable.
  GraphicsPipelineBuilder(const GraphicsPipelineBuilder&) = delete;
  GraphicsPipelineBuilder& operator=(const GraphicsPipelineBuilder&) = delete;

  // Sets a name for the pipeline. This is for debugging purpose.
  GraphicsPipelineBuilder& SetPipelineName(std::string&& name);

  // By default, depth testing and stencil testing are disabled, front face
  // direction is counter-clockwise, the rasterizer only takes one sample, and
  // primitive topology is triangle list.
  GraphicsPipelineBuilder& SetDepthTestEnabled(bool enable_test,
                                               bool enable_write);
  GraphicsPipelineBuilder& SetStencilTestEnable(bool enable_test);
  GraphicsPipelineBuilder& SetFrontFaceDirection(bool counter_clockwise);
  GraphicsPipelineBuilder& SetMultisampling(VkSampleCountFlagBits sample_count);
  GraphicsPipelineBuilder& SetPrimitiveTopology(VkPrimitiveTopology topology);

  // Adds descriptions for the vertex data bound to 'binding_point'.
  // The user does not need to set the 'binding' field of 'binding_description'
  // and 'attribute_descriptions', since it will be assigned by this function.
  // Note that 'binding_point' is not a binding number in the shader, but the
  // vertex buffer binding point used in vkCmdBindVertexBuffers().
  GraphicsPipelineBuilder& AddVertexInput(
      uint32_t binding_point,
      VkVertexInputBindingDescription&& binding_description,
      std::vector<VkVertexInputAttributeDescription>&& attribute_descriptions);

  // Sets descriptor set layouts and push constant ranges used in this pipeline.
  // Note that according to the Vulkan specification, to be compatible with all
  // devices, we only allow the user to push constants of at most 128 bytes in
  // total within one pipeline.
  GraphicsPipelineBuilder& SetPipelineLayout(
      std::vector<VkDescriptorSetLayout>&& descriptor_layouts,
      std::vector<VkPushConstantRange>&& push_constant_ranges);

  // Sets the viewport and scissor.
  GraphicsPipelineBuilder& SetViewport(ViewportInfo&& info);

  // Specifies that this pipeline will be used in the subpass of 'render_pass'
  // with 'subpass_index'.
  GraphicsPipelineBuilder& SetRenderPass(const VkRenderPass& render_pass,
                                         uint32_t subpass_index);

  // Sets color blend states for each color attachment. The length of
  // 'color_blend_states' must match the number of color attachments used in
  // the subpass.
  GraphicsPipelineBuilder& SetColorBlend(
      std::vector<VkPipelineColorBlendAttachmentState>&& color_blend_states);

  // Loads a shader that will be used at 'shader_stage' from 'file_path'.
  GraphicsPipelineBuilder& SetShader(VkShaderStageFlagBits shader_stage,
                                     std::string&& file_path);

  // Overrides.
  std::unique_ptr<Pipeline> Build() const override;

 private:
  // Refers to a subpass within a render pass.
  struct RenderPassInfo {
    VkRenderPass render_pass;
    uint32_t subpass_index;
  };

  // Specifies how to assemble primitives.
  VkPipelineInputAssemblyStateCreateInfo input_assembly_info_;

  // Configures the rasterizer state, including the front face direction and
  // face culling mode.
  VkPipelineRasterizationStateCreateInfo rasterization_info_;

  // Configures the multisampling state.
  VkPipelineMultisampleStateCreateInfo multisampling_info_;

  // Specifies whether to enable depth and/or stencil testing.
  VkPipelineDepthStencilStateCreateInfo depth_stencil_info_;

  // States that can be modified without recreating the entire pipeline.
  VkPipelineDynamicStateCreateInfo dynamic_state_info_;

  // Configures vertex input bindings and attributes.
  std::vector<VkVertexInputBindingDescription> binding_descriptions_;
  std::vector<VkVertexInputAttributeDescription> attribute_descriptions_;

  // Specifies the viewport and scissor.
  absl::optional<ViewportInfo> viewport_info_;

  // Specifies this pipeline will be used in which render pass and subpass.
  absl::optional<RenderPassInfo> render_pass_info_;

  // Color blend states of each color attachment.
  std::vector<VkPipelineColorBlendAttachmentState> color_blend_states_;

  // Maps each shader stage to the file path of shader used in that stage.
  absl::flat_hash_map<VkShaderStageFlagBits, std::string> shader_file_path_map_;
};

// The user should use this class to create compute pipelines. All internal
// states are preserved when it is used to build a pipeline, so it can be reused
// for building another pipeline. Exceptionally, by default, shader modules are
// released after building one pipeline to save the host memory. This behavior
// can be changed by the user. See class comments of ShaderModule.
class ComputePipelineBuilder : public PipelineBuilder {
 public:
  explicit ComputePipelineBuilder(SharedBasicContext context)
      : PipelineBuilder{std::move(FATAL_IF_NULL(context))} {}

  // This class is neither copyable nor movable.
  ComputePipelineBuilder(const ComputePipelineBuilder&) = delete;
  ComputePipelineBuilder& operator=(const ComputePipelineBuilder&) = delete;

  // Sets a name for the pipeline. This is for debugging purpose.
  ComputePipelineBuilder& SetPipelineName(std::string&& name);

  // Sets descriptor set layouts and push constant ranges used in this pipeline.
  // Note that according to the Vulkan specification, to be compatible with all
  // devices, we only allow the user to push constants of at most 128 bytes in
  // total within one pipeline.
  ComputePipelineBuilder& SetPipelineLayout(
      std::vector<VkDescriptorSetLayout>&& descriptor_layouts,
      std::vector<VkPushConstantRange>&& push_constant_ranges);

  // Loads a shader from 'file_path'.
  ComputePipelineBuilder& SetShader(std::string&& file_path);

  // Overrides.
  std::unique_ptr<Pipeline> Build() const override;

 private:
  // Path to shader file.
  absl::optional<std::string> shader_file_path_;
};

// VkPipeline configures multiple shader stages, multiple fixed function stages
// (including vertex input bindings and attributes, primitive assembly,
// tessellation, viewport and scissor, rasterization, multisampling, depth
// testing and stencil testing, color blending, and dynamic states), and the
// pipeline layout (including descriptor set layouts and push constant ranges).
// The user should use subclasses of PipelineBuilder to create instances of this
// class. If any state is changed, for example, the render pass and viewport may
// change if the window is resized, the user should discard the old pipeline and
// build a new one with the updated states.
class Pipeline {
 public:
  // This class is neither copyable nor movable.
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  ~Pipeline();

  // Binds to this pipeline. This should be called when 'command_buffer' is
  // recording commands.
  void Bind(const VkCommandBuffer& command_buffer) const;

  // Overloads.
  const VkPipeline& operator*() const { return pipeline_; }

  // Accessors.
  const VkPipelineLayout& layout() const { return layout_; }
  VkPipelineBindPoint binding_point() const { return binding_point_; }

 private:
  friend std::unique_ptr<Pipeline> GraphicsPipelineBuilder::Build() const;
  friend std::unique_ptr<Pipeline> ComputePipelineBuilder::Build() const;

  Pipeline(SharedBasicContext context,
           std::string name,
           const VkPipeline& pipeline,
           const VkPipelineLayout& pipeline_layout,
           VkPipelineBindPoint binding_point)
      : context_{std::move(FATAL_IF_NULL(context))}, name_{std::move(name)},
        pipeline_{pipeline}, layout_{pipeline_layout},
        binding_point_{binding_point} {}

  // Pointer to context.
  const SharedBasicContext context_;

  // Name of pipeline (used for debugging).
  const std::string name_;

  // Opaque pipeline object.
  const VkPipeline pipeline_;

  // Opaque pipeline layout object.
  const VkPipelineLayout layout_;

  // Pipeline binding point, either graphics or compute.
  const VkPipelineBindPoint binding_point_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_H */
