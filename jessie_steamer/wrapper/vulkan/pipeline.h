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
           VkPipeline&& pipeline,
           VkPipelineLayout&& pipeline_layout)
    : context_{std::move(context)},
      pipeline_{std::move(pipeline)},
      layout_{std::move(pipeline_layout)} {}
  ~Pipeline();

  // This class is neither copyable nor movable
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

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
  std::vector<VkVertexInputAttributeDescription> attrib_descriptions;
  std::vector<VkDescriptorSetLayout> descriptor_layouts;
  std::vector<ShaderModule> shader_modules;

  PipelineBuilder() = default;
  PipelineBuilder& Init(const std::shared_ptr<Context>& context);
  PipelineBuilder& set_vertex_input(
      const std::vector<VkVertexInputBindingDescription>& binding_descs,
      const std::vector<VkVertexInputAttributeDescription>& attrib_descs);
  PipelineBuilder& set_layout(
      const std::vector<VkDescriptorSetLayout>& desc_layouts);
  PipelineBuilder& set_viewport(const VkViewport& viewport);
  PipelineBuilder& set_scissor(const VkRect2D& scissor);
  PipelineBuilder& add_shader(const ShaderInfo& shader_info);
  std::unique_ptr<Pipeline> Build();
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_H */
