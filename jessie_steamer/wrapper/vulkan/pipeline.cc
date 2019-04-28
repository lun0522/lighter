//
//  pipeline.cc
//
//  Created by Pujun Lun on 2/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/pipeline.h"

#include <stdexcept>

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/context.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using common::util::nullflag;
using std::move;
using std::runtime_error;
using std::string;
using std::vector;

VkShaderModule CreateShaderModule(const SharedContext& context,
                                  const string& file) {
  const string& code = common::util::LoadTextFromFile(file);
  VkShaderModuleCreateInfo module_info{
      /*sType=*/VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      /*pNext=*/nullptr,
      nullflag,
      code.length(),
      reinterpret_cast<const uint32_t*>(code.data()),
  };

  VkShaderModule module;
  ASSERT_SUCCESS(vkCreateShaderModule(*context->device(), &module_info,
                                      context->allocator(), &module),
                 "Failed to create shader module");

  return module;
}

} /* namespace */

PipelineBuilder& PipelineBuilder::Init(const SharedContext& context) {
  this->context = context;

  input_assembly_info = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      nullflag,
      // .topology can be line, line strp, triangle fan, etc
      /*topology=*/VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      // .primitiveRestartEnable matters for drawing line/triangle strips
      /*primitiveRestartEnable=*/VK_FALSE,
  };

  rasterizer_info = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      nullflag,
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

  multisample_info = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      nullflag,
      /*rasterizationSamples=*/VK_SAMPLE_COUNT_1_BIT,
      /*sampleShadingEnable=*/VK_FALSE,
      /*minSampleShading=*/0.0f,
      /*pSampleMask=*/nullptr,
      /*alphaToCoverageEnable=*/VK_FALSE,
      /*alphaToOneEnable=*/VK_FALSE,
  };

  depth_stencil_info = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      nullflag,
      /*depthTestEnable=*/VK_TRUE,
      /*depthWriteEnable=*/VK_TRUE,  // should disable for transparent objects
      /*depthCompareOp=*/VK_COMPARE_OP_LESS_OR_EQUAL,
      // may only keep fragments in a specific depth range
      /*depthBoundsTestEnable=*/VK_FALSE,
      /*stencilTestEnable=*/VK_FALSE,  // temporarily disable
      /*front=*/VkStencilOpState{},
      /*back=*/VkStencilOpState{},
      /*minDepthBounds=*/0.0f,
      /*maxDepthBounds=*/1.0f,
  };

  // config per attached framebuffer
  color_blend_attachment = {
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
  color_blend_info = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      nullflag,
      /*logicOpEnable=*/VK_FALSE,
      /*logicOp=*/VK_LOGIC_OP_CLEAR,
      /*attachmentCount=*/1,
      &color_blend_attachment,
      /*blendConstants=*/{0.0f, 0.0f, 0.0f, 0.0f},
      // may set blend constants here
  };

  // some properties can be modified without recreating entire pipeline
  dynamic_state_info = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      nullflag,
      /*dynamicStateCount=*/0,
      /*pDynamicStates=*/nullptr,
  };

  return *this;
}

PipelineBuilder& PipelineBuilder::set_vertex_input(
    const vector<VkVertexInputBindingDescription>& binding_descriptions,
    const vector<VkVertexInputAttributeDescription>& attribute_descriptions) {
  this->binding_descriptions = binding_descriptions;
  this->attribute_descriptions = attribute_descriptions;
  vertex_input_info = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      nullflag,
      // vertex binding descriptions
      CONTAINER_SIZE(this->binding_descriptions),
      this->binding_descriptions.data(),
      // vertex attribute descriptions
      CONTAINER_SIZE(this->attribute_descriptions),
      this->attribute_descriptions.data(),
  };
  return *this;
}

PipelineBuilder& PipelineBuilder::set_layout(
    const vector<VkDescriptorSetLayout>& descriptor_layouts,
    PushConstants* push_constants) {
  this->descriptor_layouts = descriptor_layouts;
  push_constant_ranges.clear();
  if (push_constants) {
    for (const auto& info : push_constants->infos) {
      push_constant_ranges.emplace_back(VkPushConstantRange{
          push_constants->shader_stage,
          info.offset,
          info.size,
      });
    }
  }
  layout_info = {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      /*pNext=*/nullptr,
      nullflag,
      CONTAINER_SIZE(this->descriptor_layouts),
      this->descriptor_layouts.data(),
      CONTAINER_SIZE(this->push_constant_ranges),
      this->push_constant_ranges.data(),
  };
  return *this;
}

PipelineBuilder& PipelineBuilder::set_viewport(const VkViewport& viewport) {
  this->viewport = viewport;
  return *this;
}

PipelineBuilder& PipelineBuilder::set_scissor(const VkRect2D& scissor) {
  this->scissor = scissor;
  return *this;
}

PipelineBuilder& PipelineBuilder::add_shader(const ShaderInfo& shader_info) {
  shader_modules.emplace_back(shader_info.first,
                              CreateShaderModule(context, shader_info.second));
  return *this;
}

PipelineBuilder& PipelineBuilder::enable_alpha_blend() {
  color_blend_attachment.blendEnable = VK_TRUE;
  color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
  color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_blend_attachment.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
  color_blend_attachment.dstAlphaBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  return *this;
}

PipelineBuilder& PipelineBuilder::disable_depth_test() {
  depth_stencil_info.depthTestEnable = VK_FALSE;
  depth_stencil_info.depthWriteEnable = VK_FALSE;
  return *this;
}

std::unique_ptr<Pipeline> PipelineBuilder::Build() {
  if (!vertex_input_info.has_value()) {
    throw runtime_error{"Vertex input info not set"};
  }

  if (!layout_info.has_value()) {
    throw runtime_error{"Layout info not set"};
  }

  if (!viewport.has_value()) {
    throw runtime_error{"Viewport not set"};
  }

  if (!scissor.has_value()) {
    throw runtime_error{"Scissor not set"};
  }

  const VkDevice &device = *context->device();
  const VkAllocationCallbacks *allocator = context->allocator();

  VkPipelineLayout pipeline_layout;
  ASSERT_SUCCESS(vkCreatePipelineLayout(device, &layout_info.value(), allocator,
                                        &pipeline_layout),
                 "Failed to create pipeline layout");

  vector<VkPipelineShaderStageCreateInfo> shader_stages;
  shader_stages.reserve(shader_modules.size());
  for (const auto& module : shader_modules) {
    shader_stages.emplace_back(VkPipelineShaderStageCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        /*pNext=*/nullptr,
        nullflag,
        module.first,
        module.second,
        /*pName=*/"main",  // entry point of this shader
        // may use .pSpecializationInfo to specify shader constants
        /*pSpecializationInfo=*/nullptr,
    });
  }

  VkPipelineViewportStateCreateInfo viewport_info{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      nullflag,
      /*viewportCount=*/1,
      &viewport.value(),
      /*scissorCount=*/1,
      &scissor.value(),
  };

  VkGraphicsPipelineCreateInfo pipeline_info{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      /*pNext=*/nullptr,
      nullflag,
      CONTAINER_SIZE(shader_stages),
      shader_stages.data(),
      &vertex_input_info.value(),
      &input_assembly_info,
      /*pTessellationState=*/nullptr,
      &viewport_info,
      &rasterizer_info,
      &multisample_info,
      &depth_stencil_info,
      &color_blend_info,
      &dynamic_state_info,
      pipeline_layout,
      *context->render_pass(),
      /*subpass=*/0, // index of subpass where pipeline will be used
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
  for (auto& module : shader_modules) {
    vkDestroyShaderModule(device, module.second, allocator);
  }
  shader_modules.clear();

  return absl::make_unique<Pipeline>(
      context, move(pipeline), move(pipeline_layout));
}

Pipeline::~Pipeline() {
  vkDestroyPipeline(*context_->device(), pipeline_, context_->allocator());
  vkDestroyPipelineLayout(*context_->device(), layout_, context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
