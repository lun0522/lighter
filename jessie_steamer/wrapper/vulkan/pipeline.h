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

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class Context;

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
class Pipeline {
 public:
  Pipeline(std::shared_ptr<Context> context,
           const VkPipeline& pipeline,
           const VkPipelineLayout& pipeline_layout)
    : context_{std::move(context)},
      pipeline_{pipeline}, layout_{pipeline_layout} {}

  // This class is neither copyable nor movable
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  ~Pipeline();

  const VkPipeline& operator*()     const { return pipeline_; }
  const VkPipelineLayout& layout()  const { return layout_; }

 private:
  std::shared_ptr<Context> context_;
  VkPipeline pipeline_;
  VkPipelineLayout layout_;
};

class PipelineBuilder {
 public:
  using ShaderInfo = std::pair<VkShaderStageFlagBits, std::string>;
  using ShaderModule = std::pair<VkShaderStageFlagBits, VkShaderModule>;

  PipelineBuilder() = default;

  // This class is neither copyable nor movable
  PipelineBuilder(const PipelineBuilder&) = delete;
  PipelineBuilder& operator=(const PipelineBuilder&) = delete;

  // Init() should always be called first.
  PipelineBuilder& Init(std::shared_ptr<Context> context);

  // All these information must be set before Build().
  PipelineBuilder& set_vertex_input(
      const std::vector<VkVertexInputBindingDescription>& binding_descriptions,
      const std::vector<VkVertexInputAttributeDescription>&
          attribute_descriptions);
  PipelineBuilder& set_layout(
      const std::vector<VkDescriptorSetLayout>& descriptor_layouts,
      const std::vector<VkPushConstantRange>& push_constant_ranges);
  PipelineBuilder& set_viewport(VkViewport viewport);
  PipelineBuilder& set_scissor(const VkRect2D& scissor);

  // To save memory, shader modules will be released after a pipeline is built,
  // so all shaders should be added again before next Build().
  PipelineBuilder& add_shader(const ShaderInfo& shader_info);

  // By default, alpha blending is not enabled and depth testing is enabled.
  PipelineBuilder& enable_alpha_blend();
  PipelineBuilder& disable_depth_test();

  // Build() can be called multiple times.
  std::unique_ptr<Pipeline> Build();

  std::shared_ptr<Context> context;
  VkPipelineInputAssemblyStateCreateInfo input_assembly_info;
  VkPipelineRasterizationStateCreateInfo rasterizer_info;
  VkPipelineMultisampleStateCreateInfo multisample_info;
  VkPipelineDepthStencilStateCreateInfo depth_stencil_info;
  VkPipelineColorBlendAttachmentState color_blend_attachment;
  VkPipelineColorBlendStateCreateInfo color_blend_info;
  VkPipelineDynamicStateCreateInfo dynamic_state_info;
  absl::optional<VkPipelineVertexInputStateCreateInfo> vertex_input_info;
  absl::optional<VkPipelineLayoutCreateInfo> layout_info;
  absl::optional<VkViewport> viewport;
  absl::optional<VkRect2D> scissor;
  std::vector<VkVertexInputBindingDescription> binding_descriptions;
  std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
  std::vector<VkDescriptorSetLayout> descriptor_layouts;
  std::vector<VkPushConstantRange> push_constant_ranges;
  std::vector<ShaderModule> shader_modules;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_H */
