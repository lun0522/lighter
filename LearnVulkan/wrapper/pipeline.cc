//
//  pipeline.cc
//
//  Created by Pujun Lun on 2/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "pipeline.h"

#include "context.h"
#include "util.h"

namespace wrapper {
namespace vulkan {
namespace {

using std::string;

VkShaderModule CreateShaderModule(const VkDevice& device,
                                  const string& code,
                                  const VkAllocationCallbacks* allocator) {
  VkShaderModuleCreateInfo shader_module_info{
      /*sType=*/VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      code.length(),
      reinterpret_cast<const uint32_t*>(code.data()),
  };

  VkShaderModule shader_module;
  ASSERT_SUCCESS(vkCreateShaderModule(
                     device, &shader_module_info, allocator, &shader_module),
                 "Failed to create shader module");

  return shader_module;
}

} /* namespace */

void Pipeline::Init(
    std::shared_ptr<Context> context,
    const string& vert_file,
    const string& frag_file,
    const UniformBuffer& uniform_buffer,
    const std::vector<VkVertexInputBindingDescription>& binding_descs,
    const std::vector<VkVertexInputAttributeDescription>& attrib_descs) {
  context_ = context;
  vert_file_ = vert_file;
  frag_file_ = frag_file;
  const VkDevice& device = *context_->device();
  const VkAllocationCallbacks* allocator = context_->allocator();

  const string& vert_code = util::ReadFile(vert_file_);
  const string& frag_code = util::ReadFile(frag_file_);

  VkShaderModule vert_shader_module =
      CreateShaderModule(device, vert_code, allocator);
  VkShaderModule frag_shader_module =
      CreateShaderModule(device, frag_code, allocator);

  VkPipelineShaderStageCreateInfo vert_shader_info{
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      /*stage=*/VK_SHADER_STAGE_VERTEX_BIT,
      vert_shader_module,
      /*pName=*/"main", // entry point of this shader
      /*pSpecializationInfo=*/nullptr,
      // may use .pSpecializationInfo to specify shader constants
  };

  VkPipelineShaderStageCreateInfo frag_shader_info{
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      /*stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
      frag_shader_module,
      /*pName=*/"main", // entry point of this shader
      /*pSpecializationInfo=*/nullptr,
  };

  VkPipelineShaderStageCreateInfo shader_infos[]{
      vert_shader_info,
      frag_shader_info,
  };

  VkPipelineVertexInputStateCreateInfo vertex_input_info{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      // vertex binding descriptions
      CONTAINER_SIZE(binding_descs),
      binding_descs.data(),
      // vertex attribute descriptions
      CONTAINER_SIZE(attrib_descs),
      attrib_descs.data(),
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly_info{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      // .topology can be line, line strp, triangle fan, etc
      /*topology=*/VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      // .primitiveRestartEnable matters for drawing line/triangle strips
      /*primitiveRestartEnable=*/false,
  };

  VkExtent2D target_extent = context_->swapchain().extent();

  VkViewport viewport{
      /*x=*/0.0f,
      /*y=*/0.0f,
      static_cast<float>(target_extent.width),
      static_cast<float>(target_extent.height),
      /*minDepth=*/0.0f,
      /*maxDepth=*/1.0f,
  };

  VkRect2D scissor{
      /*offset=*/{0, 0},
      target_extent,
  };

  VkPipelineViewportStateCreateInfo viewport_info{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      /*viewportCount=*/1,
      &viewport,
      /*scissorCount=*/1,
      &scissor,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer_info{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
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

  VkPipelineMultisampleStateCreateInfo multisample_info{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      /*rasterizationSamples=*/VK_SAMPLE_COUNT_1_BIT,
      /*sampleShadingEnable=*/VK_FALSE,
      /*minSampleShading=*/0.0f,
      /*pSampleMask=*/nullptr,
      /*alphaToCoverageEnable=*/VK_FALSE,
      /*alphaToOneEnable=*/VK_FALSE,
  };

  // config per attached framebuffer
  VkPipelineColorBlendAttachmentState color_blend_attachment{
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
  VkPipelineColorBlendStateCreateInfo color_blend_info{
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      /*logicOpEnable=*/VK_FALSE,
      /*logicOp=*/VK_LOGIC_OP_CLEAR,
      /*attachmentCount=*/1,
      &color_blend_attachment,
      /*blendConstants=*/{0.0f, 0.0f, 0.0f, 0.0f},
      // may set blend constants here
  };

  // some properties can be modified without recreating entire pipeline
  VkPipelineDynamicStateCreateInfo dynamic_state_info{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      /*dynamicStateCount=*/0,
      /*pDynamicStates=*/nullptr,
  };

  // used to set uniform values
  const auto& descriptor_set_layouts =
      uniform_buffer.descriptor_set_layouts();
  VkPipelineLayoutCreateInfo layout_info{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      // set layouts
      CONTAINER_SIZE(descriptor_set_layouts),
      descriptor_set_layouts.data(),
      /*pushConstantRangeCount=*/0,
      /*pPushConstantRanges=*/nullptr,
  };

  ASSERT_SUCCESS(vkCreatePipelineLayout(
                     device, &layout_info, allocator, &pipeline_layout_),
                 "Failed to create pipeline layout");

  VkGraphicsPipelineCreateInfo pipeline_info{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      /*stageCount=*/2,
      shader_infos,
      &vertex_input_info,
      &input_assembly_info,
      /*pTessellationState=*/nullptr,
      &viewport_info,
      &rasterizer_info,
      &multisample_info,
      /*pDepthStencilState=*/nullptr,
      &color_blend_info,
      &dynamic_state_info,
      pipeline_layout_,
      *context_->render_pass(),
      /*subpass=*/0, // index of subpass where pipeline will be used
      /*basePipelineHandle=*/VK_NULL_HANDLE,
      /*basePipelineIndex=*/0,
      // .basePipelineHandle can be used to copy settings from another piepeline
  };

  ASSERT_SUCCESS(
      vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info,
                                allocator, &pipeline_),
      "Failed to create graphics pipeline");

  vkDestroyShaderModule(device, vert_shader_module, allocator);
  vkDestroyShaderModule(device, frag_shader_module, allocator);
}

void Pipeline::Cleanup() {
  vkDestroyPipeline(*context_->device(), pipeline_, context_->allocator());
  vkDestroyPipelineLayout(*context_->device(), pipeline_layout_,
                          context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
