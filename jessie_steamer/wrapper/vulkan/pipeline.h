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
class ShaderModule {
 public:
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

// The user should use this class to create Pipeline. All internal states are
// preserved when it is used to build a pipeline, so it can be reused later.
// Shader modules can be destroyed after the pipeline is built in order to save
// the host memory. The user may control whether this should happen. One way is
// to use AutoReleaseShaderPool; another way is to use SetShaderResourcePolicy()
// and ReleaseUnusedShaders() for fine-grained control.
class PipelineBuilder {
 public:
  // Shader modules are reference counted.
  using RefCountedShaderModule = common::RefCountedObject<ShaderModule>;

  // An instance of this will preserve all shader modules created within its
  // scope, and release them once it goes out of scope.
  using AutoReleaseShaderPool = common::AutoReleasePool<ShaderModule>;

  // Describes a viewport transformation.
  struct ViewportInfo {
    VkViewport viewport;
    VkRect2D scissor;
  };

  // Internal states will be filled with default settings, unless they are of
  // absl::optional or std::vector types.
  explicit PipelineBuilder(SharedBasicContext context);

  // This class is neither copyable nor movable.
  PipelineBuilder(const PipelineBuilder&) = delete;
  PipelineBuilder& operator=(const PipelineBuilder&) = delete;

  // Sets a name for pipeline. This is for debugging purpose.
  PipelineBuilder& SetName(std::string&& name);

  // By default, depth testing and stencil testing are disabled, front face
  // direction is counter-clockwise, and the rasterizer only takes one sample.
  PipelineBuilder& SetDepthTestEnabled(bool enable_test, bool enable_write);
  PipelineBuilder& SetStencilTestEnable(bool enable_test);
  PipelineBuilder& SetFrontFaceDirection(bool counter_clockwise);
  PipelineBuilder& SetMultisampling(VkSampleCountFlagBits sample_count);

  // Adds descriptions for the vertex data bound to 'binding_point'.
  // The user does not need to set the 'binding' field of 'binding_description'
  // and 'attribute_descriptions', since it will be assigned by this function.
  // Note that 'binding_point' is not a binding number in the shader, but the
  // vertex buffer binding point used in vkCmdBindVertexBuffers().
  PipelineBuilder& AddVertexInput(
      uint32_t binding_point,
      VkVertexInputBindingDescription&& binding_description,
      std::vector<VkVertexInputAttributeDescription>&& attribute_descriptions);

  // Sets descriptor set layouts and push constant ranges used in this pipeline.
  // Note that according to the Vulkan specification, to be compatible with all
  // devices, we only allow the user to push constants of at most 128 bytes in
  // total within one pipeline.
  PipelineBuilder& SetPipelineLayout(
      std::vector<VkDescriptorSetLayout>&& descriptor_layouts,
      std::vector<VkPushConstantRange>&& push_constant_ranges);

  // Sets the viewport and scissor.
  PipelineBuilder& SetViewport(ViewportInfo&& info);

  // Specifies that this pipeline will be used in the subpass of 'render_pass'
  // with 'subpass_index'.
  PipelineBuilder& SetRenderPass(const VkRenderPass& render_pass,
                                 uint32_t subpass_index);

  // Sets color blend states for each color attachment. The length of
  // 'color_blend_states' must match the number of color attachments used in
  // the subpass.
  PipelineBuilder& SetColorBlend(
      std::vector<VkPipelineColorBlendAttachmentState>&& color_blend_states);

  // Sets whether shader modules should be destroyed after the pipeline is
  // built in order to save the host memory. By default this is true.
  static void SetShaderResourcePolicy(bool destroy_if_unused) {
    RefCountedShaderModule::SetPolicy(destroy_if_unused);
  }

  // Releases shader modules that are currently unused.
  static void ReleaseUnusedShaders() {
    RefCountedShaderModule::ReleaseUnusedObjects();
  }

  // Loads a shader that will be used at 'shader_stage' from 'file_path'.
  PipelineBuilder& SetShader(VkShaderStageFlagBits shader_stage,
                             std::string&& file_path);

  // Returns a pipeline. This can be called multiple times.
  std::unique_ptr<Pipeline> Build() const;

 private:
  // Refers to a subpass within a render pass.
  struct RenderPassInfo {
    VkRenderPass render_pass;
    uint32_t subpass_index;
  };

  // Pointer to context.
  const SharedBasicContext context_;

  // Name of pipeline (used for debugging).
  std::string name_;

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

  // Descriptor sets and push constants determine the layout of the pipeline.
  absl::optional<VkPipelineLayoutCreateInfo> pipeline_layout_info_;
  std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  std::vector<VkPushConstantRange> push_constant_ranges_;

  // Specifies the viewport and scissor.
  absl::optional<ViewportInfo> viewport_info_;

  // Specifies this pipeline will be used in which render pass and subpass.
  absl::optional<RenderPassInfo> render_pass_info_;

  // Color blend states of each color attachment.
  std::vector<VkPipelineColorBlendAttachmentState> color_blend_states_;

  // Maps each shader stage to the file path of shader used in that stage.
  absl::flat_hash_map<VkShaderStageFlagBits, std::string> shader_file_path_map_;
};

// VkPipeline configures multiple shader stages, multiple fixed function stages
// (including vertex input bindings and attributes, primitive assembly,
// tessellation, viewport and scissor, rasterization, multisampling, depth
// testing and stencil testing, color blending, and dynamic states), and the
// pipeline layout (including descriptor set layouts and push constant ranges).
// The user should use PipelineBuilder to create instances of this class.
// If any state is changed, for example, the render pass and viewport may change
// if the window is resized, the user should discard the old pipeline and build
// a new one with the updated states.
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

 private:
  friend std::unique_ptr<Pipeline> PipelineBuilder::Build() const;

  Pipeline(SharedBasicContext context,
           std::string name,
           const VkPipeline& pipeline,
           const VkPipelineLayout& pipeline_layout)
      : context_{std::move(context)}, name_{std::move(name)},
        pipeline_{pipeline}, layout_{pipeline_layout} {}

  // Pointer to context.
  const SharedBasicContext context_;

  // Name of pipeline (used for debugging).
  const std::string name_;

  // Opaque pipeline object.
  VkPipeline pipeline_;

  // Opaque pipeline layout object.
  VkPipelineLayout layout_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_H */
