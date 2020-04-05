//
//  pipeline.cc
//
//  Created by Pujun Lun on 2/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/pipeline.h"

#include <numeric>

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/strings/str_join.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::vector;

// Creates a viewport state given 'viewport_info'.
VkPipelineViewportStateCreateInfo CreateViewportStateInfo(
    const GraphicsPipelineBuilder::ViewportInfo& viewport_info) {
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

// Creates a vertex input state. The user is responsible for keeping the
// existence of 'attribute_descriptions' and 'attribute_descriptions' until the
// returned value is no longer used.
VkPipelineVertexInputStateCreateInfo CreateVertexInputInfo(
    const vector<VkVertexInputBindingDescription>& binding_descriptions,
    const vector<VkVertexInputAttributeDescription>& attribute_descriptions) {
  return VkPipelineVertexInputStateCreateInfo{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      CONTAINER_SIZE(binding_descriptions),
      binding_descriptions.data(),
      CONTAINER_SIZE(attribute_descriptions),
      attribute_descriptions.data(),
  };
}

// Contains a loaded shader 'module' that will be used at 'stage'.
struct ShaderStage {
  VkShaderStageFlagBits stage;
  ShaderModule::RefCountedShaderModule module;
};

// Loads shaders in 'shader_file_path_map'.
vector<ShaderStage> CreateShaderStages(
    const SharedBasicContext& context,
    const absl::flat_hash_map<VkShaderStageFlagBits, std::string>&
        shader_file_path_map) {
  vector<ShaderStage> shader_stages;
  shader_stages.reserve(shader_file_path_map.size());
  for (const auto& pair : shader_file_path_map) {
    const auto& file_path = pair.second;
    shader_stages.emplace_back(ShaderStage{
        /*stage=*/pair.first,
        ShaderModule::RefCountedShaderModule::Get(
            /*identifier=*/file_path, context, file_path),
    });
  }
  return shader_stages;
}

// Extracts shader stage infos, assuming the entry point of each shader is a
// main() function. The user is responsible for keeping the existence of
// 'shader_stages' until the returned value is no longer used.
vector<VkPipelineShaderStageCreateInfo> CreateShaderStageInfos(
    const vector<ShaderStage>& shader_stages) {
  static constexpr char kShaderEntryPoint[] = "main";
  vector<VkPipelineShaderStageCreateInfo> shader_stage_infos;
  shader_stage_infos.reserve(shader_stages.size());
  for (const auto& stage : shader_stages) {
    shader_stage_infos.emplace_back(VkPipelineShaderStageCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        /*pNext=*/nullptr,
        /*flags=*/nullflag,
        stage.stage,
        **stage.module,
        /*pName=*/kShaderEntryPoint,
        // May use 'pSpecializationInfo' to specify shader constants.
        /*pSpecializationInfo=*/nullptr,
    });
  }
  return shader_stage_infos;
}

} /* namespace */

ShaderModule::ShaderModule(SharedBasicContext context,
                           const std::string& file_path)
    : context_{std::move(FATAL_IF_NULL(context))} {
  context_->RegisterAutoReleasePool<RefCountedShaderModule>("shader");

  const auto raw_data = absl::make_unique<common::RawData>(file_path);
  const VkShaderModuleCreateInfo module_info{
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      raw_data->size,
      reinterpret_cast<const uint32_t*>(raw_data->data),
  };
  ASSERT_SUCCESS(vkCreateShaderModule(*context_->device(), &module_info,
                                      *context_->allocator(), &shader_module_),
                 "Failed to create shader module");
}

GraphicsPipelineBuilder::GraphicsPipelineBuilder(SharedBasicContext context)
    : context_{std::move(FATAL_IF_NULL(context))} {
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

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetName(std::string&& name) {
  name_ = std::move(name);
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetDepthTestEnabled(
    bool enable_test, bool enable_write) {
  depth_stencil_info_.depthTestEnable = util::ToVkBool(enable_test);
  depth_stencil_info_.depthWriteEnable = util::ToVkBool(enable_write);
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetStencilTestEnable(
    bool enable_test) {
  depth_stencil_info_.stencilTestEnable = util::ToVkBool(enable_test);
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetFrontFaceDirection(
    bool counter_clockwise) {
  rasterization_info_.frontFace = counter_clockwise
                                      ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                                      : VK_FRONT_FACE_CLOCKWISE;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetMultisampling(
    VkSampleCountFlagBits sample_count) {
  multisampling_info_.rasterizationSamples = sample_count;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetPrimitiveTopology(
    VkPrimitiveTopology topology) {
  input_assembly_info_.topology = topology;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddVertexInput(
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

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetPipelineLayout(
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

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetViewport(
    ViewportInfo&& info) {
  // Flip the viewport as suggested by:
  // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport
  VkViewport& viewport = info.viewport;
  viewport.y += viewport.height;
  viewport.height *= -1;
  viewport_info_.emplace(info);
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetRenderPass(
    const VkRenderPass& render_pass, uint32_t subpass_index) {
  render_pass_info_.emplace(RenderPassInfo{render_pass, subpass_index});
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetColorBlend(
    vector<VkPipelineColorBlendAttachmentState>&& color_blend_states) {
  color_blend_states_ = std::move(color_blend_states);
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetShader(
    VkShaderStageFlagBits shader_stage, std::string&& file_path) {
  shader_file_path_map_[shader_stage] = std::move(file_path);
  return *this;
}

std::unique_ptr<Pipeline> GraphicsPipelineBuilder::Build() const {
  ASSERT_HAS_VALUE(pipeline_layout_info_, "Pipeline layout is not set");
  ASSERT_HAS_VALUE(viewport_info_, "Viewport is not set");
  ASSERT_HAS_VALUE(render_pass_info_, "Render pass is not set");
  ASSERT_NON_EMPTY(color_blend_states_, "Color blend is not set");
  ASSERT_NON_EMPTY(shader_file_path_map_, "Shader is not set");

  const auto viewport_state_info =
      CreateViewportStateInfo(viewport_info_.value());
  const auto color_blend_info = CreateColorBlendInfo(color_blend_states_);
  const auto vertex_input_info = CreateVertexInputInfo(binding_descriptions_,
                                                       attribute_descriptions_);
  // Shader modules can be destroyed to save the host memory after the pipeline
  // is created.
  const auto shader_stages =
      CreateShaderStages(context_, shader_file_path_map_);
  const auto shader_stage_infos = CreateShaderStageInfos(shader_stages);

  VkPipelineLayout pipeline_layout;
  ASSERT_SUCCESS(
      vkCreatePipelineLayout(
          *context_->device(), &pipeline_layout_info_.value(),
          *context_->allocator(), &pipeline_layout),
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
      vkCreateGraphicsPipelines(
          *context_->device(), /*pipelineCache=*/VK_NULL_HANDLE,
          /*createInfoCount=*/1, &pipeline_info, *context_->allocator(),
          &pipeline),
      "Failed to create graphics pipeline");

  return std::unique_ptr<Pipeline>{
      new Pipeline{context_, name_, pipeline, pipeline_layout}};
}

void Pipeline::Bind(const VkCommandBuffer& command_buffer) const {
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
}

Pipeline::~Pipeline() {
  vkDestroyPipeline(*context_->device(), pipeline_, *context_->allocator());
  vkDestroyPipelineLayout(*context_->device(), layout_, *context_->allocator());
#ifndef NDEBUG
  LOG_INFO << absl::StreamFormat("Pipeline '%s' destructed", name_);
#endif  /* !NDEBUG */
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
