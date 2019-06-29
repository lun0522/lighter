//
//  pipeline.cc
//
//  Created by Pujun Lun on 2/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/pipeline.h"

#include <memory>

#include "absl/memory/memory.h"
#include "jessie_steamer/common/file.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::vector;

VkShaderModule CreateShaderModule(const SharedBasicContext& context,
                                  const std::string& path) {
  const auto raw_data = absl::make_unique<common::RawData>(path);
  VkShaderModuleCreateInfo module_info{
      /*sType=*/VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      raw_data->size,
      reinterpret_cast<const uint32_t*>(raw_data->data),
  };

  VkShaderModule module;
  ASSERT_SUCCESS(vkCreateShaderModule(*context->device(), &module_info,
                                      context->allocator(), &module),
                 "Failed to create shader module");

  return module;
}

vector<VkPipelineShaderStageCreateInfo> CreateShaderStageInfos(
    const vector<PipelineBuilder::ShaderModule>& shader_modules) {
  vector<VkPipelineShaderStageCreateInfo> shader_stages;
  shader_stages.reserve(shader_modules.size());
  for (const auto& module : shader_modules) {
    shader_stages.emplace_back(VkPipelineShaderStageCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        /*pNext=*/nullptr,
        /*flags=*/nullflag,
        module.first,
        module.second,
        /*pName=*/"main",  // entry point of this shader
        // may use .pSpecializationInfo to specify shader constants
        /*pSpecializationInfo=*/nullptr,
    });
  }
  return shader_stages;
}

VkPipelineViewportStateCreateInfo CreateViewportStateInfo(
    const PipelineBuilder::ViewportInfo& viewport_info) {
  return VkPipelineViewportStateCreateInfo{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      /*viewportCount=*/1,
      &viewport_info.first,
      /*scissorCount=*/1,
      &viewport_info.second,
  };
}

} /* namespace */

PipelineBuilder::PipelineBuilder(SharedBasicContext context)
    : context_{std::move(context)} {
  input_assembly_info_ = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      // .topology can be line, line strp, triangle fan, etc
      /*topology=*/VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      // .primitiveRestartEnable matters for drawing line/triangle strips
      /*primitiveRestartEnable=*/VK_FALSE,
  };

  rasterizer_info_ = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      // fragments beyond clip space will be discarded, not clamped
      /*depthClampEnable=*/VK_FALSE,
      // disable outputs to framebuffer if TRUE
      /*rasterizerDiscardEnable=*/VK_FALSE,
      // fill polygons with fragments
      /*polygonMode=*/VK_POLYGON_MODE_FILL,
      /*cullMode=*/VK_CULL_MODE_BACK_BIT,
      /*frontFace=*/VK_FRONT_FACE_COUNTER_CLOCKWISE,
      // don't let rasterizer alter depth values
      /*depthBiasEnable=*/VK_FALSE,
      /*depthBiasConstantFactor=*/0.0f,
      /*depthBiasClamp=*/0.0f,
      /*depthBiasSlopeFactor=*/0.0f,
      /*lineWidth=*/1.0f,
  };

  multisample_info_ = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      /*rasterizationSamples=*/VK_SAMPLE_COUNT_1_BIT,
      /*sampleShadingEnable=*/VK_FALSE,
      /*minSampleShading=*/0.0f,
      /*pSampleMask=*/nullptr,
      /*alphaToCoverageEnable=*/VK_FALSE,
      /*alphaToOneEnable=*/VK_FALSE,
  };

  depth_stencil_info_ = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      /*depthTestEnable=*/VK_FALSE,
      /*depthWriteEnable=*/VK_FALSE,
      /*depthCompareOp=*/VK_COMPARE_OP_LESS_OR_EQUAL,
      // may only keep fragments in a specific depth range
      /*depthBoundsTestEnable=*/VK_FALSE,
      /*stencilTestEnable=*/VK_FALSE,
      /*front=*/VkStencilOpState{},
      /*back=*/VkStencilOpState{},
      /*minDepthBounds=*/0.0f,
      /*maxDepthBounds=*/1.0f,
  };

  // config per attached framebuffer
  color_blend_attachment_ = {
      /*blendEnable=*/VK_FALSE,
      /*srcColorBlendFactor=*/VK_BLEND_FACTOR_ZERO,
      /*dstColorBlendFactor=*/VK_BLEND_FACTOR_ZERO,
      /*colorBlendOp=*/VK_BLEND_OP_ADD,
      /*srcAlphaBlendFactor=*/VK_BLEND_FACTOR_ZERO,
      /*dstAlphaBlendFactor=*/VK_BLEND_FACTOR_ZERO,
      /*alphaBlendOp=*/VK_BLEND_OP_ADD,
      /*colorWriteMask=*/VK_COLOR_COMPONENT_R_BIT
                             | VK_COLOR_COMPONENT_G_BIT
                             | VK_COLOR_COMPONENT_B_BIT
                             | VK_COLOR_COMPONENT_A_BIT,
  };

  // global color blending settings
  color_blend_info_ = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      /*logicOpEnable=*/VK_FALSE,
      /*logicOp=*/VK_LOGIC_OP_CLEAR,
      /*attachmentCount=*/1,
      &color_blend_attachment_,
      /*blendConstants=*/{0.0f, 0.0f, 0.0f, 0.0f},
      // may set blend constants here
  };

  // some properties can be modified without recreating entire pipeline
  dynamic_state_info_ = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      /*dynamicStateCount=*/0,
      /*pDynamicStates=*/nullptr,
  };
}

PipelineBuilder& PipelineBuilder::set_vertex_input(
    vector<VkVertexInputBindingDescription>&& binding_descriptions,
    vector<VkVertexInputAttributeDescription>&& attribute_descriptions) {
  binding_descriptions_ = std::move(binding_descriptions);
  attribute_descriptions_ = std::move(attribute_descriptions);
  vertex_input_info_.emplace(VkPipelineVertexInputStateCreateInfo{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      // vertex binding descriptions
      CONTAINER_SIZE(binding_descriptions_),
      binding_descriptions_.data(),
      // vertex attribute descriptions
      CONTAINER_SIZE(attribute_descriptions_),
      attribute_descriptions_.data(),
  });
  return *this;
}

PipelineBuilder& PipelineBuilder::set_layout(
    vector<VkDescriptorSetLayout>&& descriptor_layouts,
    vector<VkPushConstantRange>&& push_constant_ranges) {
  descriptor_layouts_ = std::move(descriptor_layouts);
  push_constant_ranges_ = std::move(push_constant_ranges);
  layout_info_.emplace(VkPipelineLayoutCreateInfo{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      CONTAINER_SIZE(descriptor_layouts_),
      descriptor_layouts_.data(),
      CONTAINER_SIZE(push_constant_ranges_),
      push_constant_ranges_.data(),
  });
  return *this;
}

PipelineBuilder& PipelineBuilder::set_viewport(ViewportInfo&& info) {
  viewport_info_.emplace(std::move(info));
  // flip viewport as suggested by:
  // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport
  auto& viewport = viewport_info_->first;
  float height = viewport.y - viewport.height;
  viewport.y += viewport.height;
  viewport.height = height;
  return *this;
}

PipelineBuilder& PipelineBuilder::set_render_pass(const RenderPass& render_pass,
                                                  uint32_t subpass_index) {
  render_pass_info_.emplace(*render_pass, subpass_index);
  return *this;
}

PipelineBuilder& PipelineBuilder::add_shader(const ShaderInfo& info) {
  shader_modules_.emplace_back(info.first,
                               CreateShaderModule(context_, info.second));
  return *this;
}

PipelineBuilder& PipelineBuilder::enable_depth_test() {
  depth_stencil_info_.depthTestEnable = VK_TRUE;
  depth_stencil_info_.depthWriteEnable = VK_TRUE;
  return *this;
}

PipelineBuilder& PipelineBuilder::enable_stencil_test() {
  depth_stencil_info_.stencilTestEnable = VK_TRUE;
  return *this;
}

PipelineBuilder& PipelineBuilder::enable_alpha_blend() {
  color_blend_attachment_.blendEnable = VK_TRUE;
  color_blend_attachment_.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
  color_blend_attachment_.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_blend_attachment_.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
  color_blend_attachment_.dstAlphaBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  return *this;
}

std::unique_ptr<Pipeline> PipelineBuilder::Build() {
  ASSERT_HAS_VALUE(vertex_input_info_, "Vertex input info is not set");
  ASSERT_HAS_VALUE(layout_info_, "Layout info is not set");
  ASSERT_HAS_VALUE(viewport_info_, "Viewport is not set");
  ASSERT_HAS_VALUE(render_pass_info_, "Render pass is not set");

  const VkDevice &device = *context_->device();
  const VkAllocationCallbacks *allocator = context_->allocator();
  auto shader_stage_infos = CreateShaderStageInfos(shader_modules_);
  auto viewport_state_info = CreateViewportStateInfo(viewport_info_.value());

  VkPipelineLayout pipeline_layout;
  ASSERT_SUCCESS(vkCreatePipelineLayout(device, &layout_info_.value(),
                                        allocator, &pipeline_layout),
                 "Failed to create pipeline layout");

  VkGraphicsPipelineCreateInfo pipeline_info{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      CONTAINER_SIZE(shader_stage_infos),
      shader_stage_infos.data(),
      &vertex_input_info_.value(),
      &input_assembly_info_,
      /*pTessellationState=*/nullptr,
      &viewport_state_info,
      &rasterizer_info_,
      &multisample_info_,
      &depth_stencil_info_,
      &color_blend_info_,
      &dynamic_state_info_,
      pipeline_layout,
      /*renderPass=*/render_pass_info_.value().first,
      /*subpass=*/render_pass_info_.value().second,
      /*basePipelineHandle=*/VK_NULL_HANDLE,
      /*basePipelineIndex=*/0,
      // .basePipelineHandle can be used to copy settings from another piepeline
  };

  VkPipeline pipeline;
  ASSERT_SUCCESS(
      vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info,
                                allocator, &pipeline),
      "Failed to create graphics pipeline");

  // shader modules can be destroyed after pipeline is constructed
  for (auto& module : shader_modules_) {
    vkDestroyShaderModule(device, module.second, allocator);
  }
  shader_modules_.clear();

  return absl::make_unique<Pipeline>(context_, pipeline, pipeline_layout);
}

void Pipeline::Bind(const VkCommandBuffer& command_buffer) const {
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
}

Pipeline::~Pipeline() {
  vkDestroyPipeline(*context_->device(), pipeline_, context_->allocator());
  vkDestroyPipelineLayout(*context_->device(), layout_, context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
