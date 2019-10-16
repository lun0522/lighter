//
//  pipeline.cc
//
//  Created by Pujun Lun on 2/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/pipeline.h"

#include <numeric>

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/strings/str_join.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::vector;

// Loads the shader from 'file_path' and creates a shader module for it.
VkShaderModule CreateShaderModule(const SharedBasicContext& context,
                                  const std::string& file_path) {
  const auto raw_data = absl::make_unique<common::RawData>(file_path);
  const VkShaderModuleCreateInfo module_info{
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      raw_data->size,
      reinterpret_cast<const uint32_t*>(raw_data->data),
  };

  VkShaderModule module;
  ASSERT_SUCCESS(vkCreateShaderModule(*context->device(), &module_info,
                                      *context->allocator(), &module),
                 "Failed to create shader module");

  return module;
}

// Creates shader stages given 'shader_modules', assuming the entry point of
// each shader is a main() function.
vector<VkPipelineShaderStageCreateInfo> CreateShaderStageInfos(
    const vector<PipelineBuilder::ShaderInfo>& shader_infos) {
  static constexpr char kShaderEntryPoint[] = "main";
  vector<VkPipelineShaderStageCreateInfo> shader_stages;
  shader_stages.reserve(shader_infos.size());
  for (const auto& info : shader_infos) {
    shader_stages.emplace_back(VkPipelineShaderStageCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        /*pNext=*/nullptr,
        /*flags=*/nullflag,
        info.stage,
        info.module,
        /*pName=*/kShaderEntryPoint,
        // May use 'pSpecializationInfo' to specify shader constants.
        /*pSpecializationInfo=*/nullptr,
    });
  }
  return shader_stages;
}

// Creates a viewport state given 'viewport_info'.
VkPipelineViewportStateCreateInfo CreateViewportStateInfo(
    const PipelineBuilder::ViewportInfo& viewport_info) {
  return VkPipelineViewportStateCreateInfo{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      /*viewportCount=*/1,
      &viewport_info.viewport,
      /*scissorCount=*/1,
      &viewport_info.scissor,
  };
}

// Creates a color blend state given 'color_blend_states' of color attachments.
VkPipelineColorBlendStateCreateInfo CreateColorBlendInfo(
    const vector<VkPipelineColorBlendAttachmentState>& color_blend_states) {
  return VkPipelineColorBlendStateCreateInfo{
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      /*logicOpEnable=*/VK_FALSE,
      VK_LOGIC_OP_CLEAR,
      CONTAINER_SIZE(color_blend_states),
      color_blend_states.data(),
      /*blendConstants=*/{0.0f, 0.0f, 0.0f, 0.0f},
  };
}

} /* namespace */

PipelineBuilder::PipelineBuilder(SharedBasicContext context)
    : context_{std::move(context)} {
  input_assembly_info_ = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      // 'topology' can be line, line strip, triangle fan, etc.
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      // 'primitiveRestartEnable' matters for drawing line/triangle strips.
      /*primitiveRestartEnable=*/VK_FALSE,
  };

  rasterization_info_ = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      // If false, fragments beyond clip space will be discarded, not clamped.
      /*depthClampEnable=*/VK_FALSE,
      // If true, disable outputs to the framebuffer.
      /*rasterizerDiscardEnable=*/VK_FALSE,
      // Fill polygons with fragments.
      VK_POLYGON_MODE_FILL,
      VK_CULL_MODE_BACK_BIT,
      VK_FRONT_FACE_COUNTER_CLOCKWISE,
      // Whether to let the rasterizer alter depth values.
      /*depthBiasEnable=*/VK_FALSE,
      /*depthBiasConstantFactor=*/0.0f,
      /*depthBiasClamp=*/0.0f,
      /*depthBiasSlopeFactor=*/0.0f,
      /*lineWidth=*/1.0f,
  };

  multisampling_info_ = {
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
      // We may only keep fragments in a specific depth range.
      /*depthBoundsTestEnable=*/VK_FALSE,
      /*stencilTestEnable=*/VK_FALSE,
      /*front=*/VkStencilOpState{},
      /*back=*/VkStencilOpState{},
      /*minDepthBounds=*/0.0f,
      /*maxDepthBounds=*/1.0f,
  };

  dynamic_state_info_ = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      /*dynamicStateCount=*/0,
      /*pDynamicStates=*/nullptr,
  };
}

PipelineBuilder& PipelineBuilder::SetDepthTestEnabled(bool enable_test) {
  depth_stencil_info_.depthTestEnable = util::ToVkBool(enable_test);
  depth_stencil_info_.depthWriteEnable = util::ToVkBool(enable_test);
  return *this;
}

PipelineBuilder& PipelineBuilder::SetStencilTestEnable(bool enable_test) {
  depth_stencil_info_.stencilTestEnable = util::ToVkBool(enable_test);
  return *this;
}

PipelineBuilder& PipelineBuilder::SetFrontFaceDirection(
    bool counter_clockwise) {
  rasterization_info_.frontFace = counter_clockwise
                                      ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                                      : VK_FRONT_FACE_CLOCKWISE;
  return *this;
}

PipelineBuilder& PipelineBuilder::SetMultisampling(
    VkSampleCountFlagBits sample_count) {
  multisampling_info_.rasterizationSamples = sample_count;
  return *this;
}

PipelineBuilder& PipelineBuilder::AddVertexInput(
    uint32_t binding_point,
    VkVertexInputBindingDescription&& binding_description,
    vector<VkVertexInputAttributeDescription>&& attribute_descriptions) {
  binding_description.binding = binding_point;
  for (auto& description : attribute_descriptions) {
    description.binding = binding_point;
  }
  binding_descriptions_.emplace_back(std::move(binding_description));
  common::util::VectorAppend(&attribute_descriptions_, &attribute_descriptions);
  return *this;
}

PipelineBuilder& PipelineBuilder::SetPipelineLayout(
    vector<VkDescriptorSetLayout>&& descriptor_layouts,
    vector<VkPushConstantRange>&& push_constant_ranges) {
  // Make sure no more than 128 bytes constants are pushed in this pipeline.
  vector<int> push_constant_sizes(push_constant_ranges.size());
  for (int i = 0; i < push_constant_ranges.size(); ++i) {
    push_constant_sizes[i] = push_constant_ranges[i].size;
  }
  const auto total_push_constant_size =
      std::accumulate(push_constant_sizes.begin(),
                      push_constant_sizes.end(), 0);
  ASSERT_TRUE(total_push_constant_size <= kMaxPushConstantSize,
              absl::StrFormat(
                  "Pushing constant of total size %d bytes in the pipeline "
                  "(break down: %s). To be compatible with all devices, the "
                  "total size should not be greater than %d bytes.",
                  total_push_constant_size,
                  absl::StrJoin(push_constant_sizes, " + "),
                  kMaxPushConstantSize));

  descriptor_layouts_ = std::move(descriptor_layouts);
  push_constant_ranges_ = std::move(push_constant_ranges);
  pipeline_layout_info_.emplace(VkPipelineLayoutCreateInfo{
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

PipelineBuilder& PipelineBuilder::SetViewport(VkViewport&& viewport,
                                              VkRect2D&& scissor) {
  // Flip the viewport as suggested by:
  // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport
  const float height = viewport.y - viewport.height;
  viewport.y += viewport.height;
  viewport.height = height;
  viewport_info_.emplace(ViewportInfo{std::move(viewport), std::move(scissor)});
  return *this;
}

PipelineBuilder& PipelineBuilder::SetRenderPass(const VkRenderPass& render_pass,
                                                uint32_t subpass_index) {
  render_pass_info_.emplace(RenderPassInfo{render_pass, subpass_index});
  return *this;
}

PipelineBuilder& PipelineBuilder::SetColorBlend(
    vector<VkPipelineColorBlendAttachmentState>&& color_blend_states) {
  color_blend_states_ = std::move(color_blend_states);
  return *this;
}

PipelineBuilder& PipelineBuilder::AddShader(VkShaderStageFlagBits shader_stage,
                                            const std::string& file_path) {
  shader_infos_.emplace_back(ShaderInfo{
      shader_stage, CreateShaderModule(context_, file_path)
  });
  return *this;
}

std::unique_ptr<Pipeline> PipelineBuilder::Build() {
  ASSERT_HAS_VALUE(pipeline_layout_info_, "Pipeline layout is not set");
  ASSERT_HAS_VALUE(viewport_info_, "Viewport is not set");
  ASSERT_HAS_VALUE(render_pass_info_, "Render pass is not set");
  ASSERT_NON_EMPTY(color_blend_states_, "Color blend is not set");
  ASSERT_NON_EMPTY(shader_infos_, "Shader is not set");

  const VkDevice& device = *context_->device();
  const VkAllocationCallbacks* allocator = *context_->allocator();
  const auto shader_stage_infos = CreateShaderStageInfos(shader_infos_);
  const auto viewport_state_info =
      CreateViewportStateInfo(viewport_info_.value());
  const auto color_blend_info = CreateColorBlendInfo(color_blend_states_);

  const VkPipelineVertexInputStateCreateInfo vertex_input_info{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      CONTAINER_SIZE(binding_descriptions_),
      binding_descriptions_.data(),
      CONTAINER_SIZE(attribute_descriptions_),
      attribute_descriptions_.data(),
  };

  VkPipelineLayout pipeline_layout;
  ASSERT_SUCCESS(vkCreatePipelineLayout(device, &pipeline_layout_info_.value(),
                                        allocator, &pipeline_layout),
                 "Failed to create pipeline layout");

  const VkGraphicsPipelineCreateInfo pipeline_info{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      CONTAINER_SIZE(shader_stage_infos),
      shader_stage_infos.data(),
      &vertex_input_info,
      &input_assembly_info_,
      /*pTessellationState=*/nullptr,
      &viewport_state_info,
      &rasterization_info_,
      &multisampling_info_,
      &depth_stencil_info_,
      &color_blend_info,
      &dynamic_state_info_,
      pipeline_layout,
      render_pass_info_->render_pass,
      render_pass_info_->subpass_index,
      // 'basePipelineHandle' and 'basePipelineIndex' can be used to copy
      // settings from another pipeline.
      /*basePipelineHandle=*/VK_NULL_HANDLE,
      /*basePipelineIndex=*/0,
  };

  VkPipeline pipeline;
  ASSERT_SUCCESS(
      vkCreateGraphicsPipelines(device, /*pipelineCache=*/VK_NULL_HANDLE,
                                /*createInfoCount=*/1, &pipeline_info,
                                allocator, &pipeline),
      "Failed to create graphics pipeline");

  // Shader modules can be destroyed to save the host memory after the pipeline
  // is created.
  for (auto& info : shader_infos_) {
    vkDestroyShaderModule(device, info.module, allocator);
  }
  shader_infos_.clear();

  return std::unique_ptr<Pipeline>{
      new Pipeline{context_, pipeline, pipeline_layout}};
}

void Pipeline::Bind(const VkCommandBuffer& command_buffer) const {
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
}

Pipeline::~Pipeline() {
  vkDestroyPipeline(*context_->device(), pipeline_, *context_->allocator());
  vkDestroyPipelineLayout(*context_->device(), layout_, *context_->allocator());
#ifndef NDEBUG
  LOG << "Pipeline destructed" << std::endl;
#endif  /* !NDEBUG */
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
