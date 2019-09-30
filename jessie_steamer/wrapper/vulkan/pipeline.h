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

#include "absl/types/optional.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

/** VkPipeline stores the entire graphics pipeline.
 *
 *  Initialization:
 *    ShaderStage (vertex and fragment shaders)
 *    VertexInputState (how to interpret vertex attributes)
 *    InputAssemblyState (what topology to use)
 *    ViewportState (viewport and scissor)
 *    RasterizationState (lines, polygons, face culling, etc)
 *    MultisampleState (how many sample points)
 *    DepthStencilState
 *    ColorBlendState
 *    DynamicState (which properties of this pipeline will be dynamic)
 *    VkPipelineLayout (set uniform values)
 *    VkRenderPass and subpass
 *    BasePipeline (may copy settings from another pipeline)
 */
class Pipeline;

class PipelineBuilder {
 public:
  using ShaderInfo = std::pair<VkShaderStageFlagBits, std::string>;
  using ShaderModule = std::pair<VkShaderStageFlagBits, VkShaderModule>;
  using ViewportInfo = std::pair<VkViewport, VkRect2D>;

  explicit PipelineBuilder(SharedBasicContext context);

  // This class is neither copyable nor movable.
  PipelineBuilder(const PipelineBuilder&) = delete;
  PipelineBuilder& operator=(const PipelineBuilder&) = delete;

  // All these information must be set before Build().
  PipelineBuilder& set_vertex_input(
      std::vector<VkVertexInputBindingDescription>&& binding_descriptions,
      std::vector<VkVertexInputAttributeDescription>&& attribute_descriptions);
  PipelineBuilder& set_layout(
      std::vector<VkDescriptorSetLayout>&& descriptor_layouts,
      std::vector<VkPushConstantRange>&& push_constant_ranges);
  PipelineBuilder& set_viewport(ViewportInfo&& info);
  PipelineBuilder& set_render_pass(const VkRenderPass& render_pass,
                                   uint32_t subpass_index);
  PipelineBuilder& set_sample_count(VkSampleCountFlagBits sample_count);
  PipelineBuilder& add_shader(const ShaderInfo& info);

  // By default depth testing, stencil testing and alpha blending are disabled,
  // and front face is counter-clockwise.
  PipelineBuilder& enable_depth_test();
  PipelineBuilder& enable_stencil_test();
  PipelineBuilder& enable_alpha_blend();
  PipelineBuilder& set_front_face_clockwise();

  // Build() can be called multiple times. Note that 'shader_modules_' is
  // cleared after each call to Build() to save memory, so add_shader() should
  // be called before next call to Build().
  std::unique_ptr<Pipeline> Build();

 private:
  using RenderPassInfo = std::pair<VkRenderPass, uint32_t>;

  const SharedBasicContext context_;
  VkPipelineInputAssemblyStateCreateInfo input_assembly_info_;
  VkPipelineRasterizationStateCreateInfo rasterizer_info_;
  VkPipelineMultisampleStateCreateInfo multisample_info_;
  VkPipelineDepthStencilStateCreateInfo depth_stencil_info_;
  VkPipelineColorBlendAttachmentState color_blend_attachment_;
  VkPipelineColorBlendStateCreateInfo color_blend_info_;
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

  void Bind(const VkCommandBuffer& command_buffer) const;

  const VkPipeline& operator*()     const { return pipeline_; }
  const VkPipelineLayout& layout()  const { return layout_; }

 private:
  friend std::unique_ptr<Pipeline> PipelineBuilder::Build();

  Pipeline(SharedBasicContext context,
           const VkPipeline& pipeline,
           const VkPipelineLayout& pipeline_layout)
      : context_{std::move(context)},
        pipeline_{pipeline}, layout_{pipeline_layout} {}

  const SharedBasicContext context_;
  VkPipeline pipeline_;
  VkPipelineLayout layout_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_H */
