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
#include <utility>
#include <vector>

#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "third_party/absl/types/optional.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// Forward declarations.
class Pipeline;

// The user should use this class to create Pipeline. The internal states,
// except for shader modules, are preserved when it is used to build a pipeline,
// so the user can add shaders again and reuse the builder later.
class PipelineBuilder {
 public:
  // Shader stage and file path of the shader.
  using ShaderInfo = std::pair<VkShaderStageFlagBits, std::string>;

  // Shader stage and shader module.
  using ShaderModule = std::pair<VkShaderStageFlagBits, VkShaderModule>;

  // Viewport and scissor.
  using ViewportInfo = std::pair<VkViewport, VkRect2D>;

  explicit PipelineBuilder(SharedBasicContext context);

  // This class is neither copyable nor movable.
  PipelineBuilder(const PipelineBuilder&) = delete;
  PipelineBuilder& operator=(const PipelineBuilder&) = delete;

  // All these information must be set before calling Build().
  PipelineBuilder& SetVertexInput(
      std::vector<VkVertexInputBindingDescription>&& binding_descriptions,
      std::vector<VkVertexInputAttributeDescription>&& attribute_descriptions);
  PipelineBuilder& SetPipelineLayout(
      std::vector<VkDescriptorSetLayout>&& descriptor_layouts,
      std::vector<VkPushConstantRange>&& push_constant_ranges);
  PipelineBuilder& SetColorBlend(
      std::vector<VkPipelineColorBlendAttachmentState>&& color_blend_states);
  PipelineBuilder& SetViewport(ViewportInfo&& info);
  PipelineBuilder& SetRenderPass(const VkRenderPass& render_pass,
                                 uint32_t subpass_index);
  PipelineBuilder& SetSampleCount(VkSampleCountFlagBits sample_count);
  PipelineBuilder& AddShader(const ShaderInfo& info);

  // By default depth testing, stencil testing and alpha blending are disabled,
  // and front face is counter-clockwise.
  PipelineBuilder& EnableDepthTest();
  PipelineBuilder& EnableStencilTest();
  PipelineBuilder& SetFrontFaceClockwise();

  // Build() can be called multiple times. Note that 'shader_modules_' is
  // cleared after each call to Build() to save memory, so AddShader() should
  // be called for all shaders before the next call to Build().
  std::unique_ptr<Pipeline> Build();

 private:
  // Render pass and subpass index where this pipeline will be used.
  using RenderPassInfo = std::pair<VkRenderPass, uint32_t>;

  // Pointer to context.
  const SharedBasicContext context_;

  // Following members are required to build the pipeline.
  VkPipelineInputAssemblyStateCreateInfo input_assembly_info_;
  VkPipelineRasterizationStateCreateInfo rasterizer_info_;
  VkPipelineMultisampleStateCreateInfo multisample_info_;
  VkPipelineDepthStencilStateCreateInfo depth_stencil_info_;
  std::vector<VkPipelineColorBlendAttachmentState> color_blend_states_;
  VkPipelineDynamicStateCreateInfo dynamic_state_info_;
  absl::optional<VkPipelineVertexInputStateCreateInfo> vertex_input_info_;
  absl::optional<VkPipelineLayoutCreateInfo> layout_info_;
  absl::optional<ViewportInfo> viewport_info_;
  absl::optional<RenderPassInfo> render_pass_info_;
  std::vector<VkVertexInputBindingDescription> binding_descriptions_;
  std::vector<VkVertexInputAttributeDescription> attribute_descriptions_;
  std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  std::vector<VkPushConstantRange> push_constant_ranges_;
  std::vector<ShaderModule> shader_modules_;
};

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
  friend std::unique_ptr<Pipeline> PipelineBuilder::Build();

  Pipeline(SharedBasicContext context,
           const VkPipeline& pipeline,
           const VkPipelineLayout& pipeline_layout)
      : context_{std::move(context)},
        pipeline_{pipeline}, layout_{pipeline_layout} {}

  // Pointer to context.
  const SharedBasicContext context_;

  // Opaque pipeline object.
  VkPipeline pipeline_;

  // Opaque pipeline layout object.
  VkPipelineLayout layout_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_H */
