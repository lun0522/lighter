//
//  pipeline.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 2/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "pipeline.h"

#include "application.h"
#include "buffer.h"
#include "triangle_app.h"
#include "util.h"

using namespace std;

namespace vulkan {

namespace {

VkShaderModule CreateShaderModule(const VkDevice& device,
                                  const string& code) {
  VkShaderModuleCreateInfo shader_module_info{
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = code.length(),
    .pCode = reinterpret_cast<const uint32_t*>(code.data()),
  };

  VkShaderModule shader_module{};
  ASSERT_SUCCESS(vkCreateShaderModule(
                     device, &shader_module_info, nullptr, &shader_module),
                 "Failed to create shader module");

  return shader_module;
}

} /* namespace */

void Pipeline::Init() {
  const VkDevice& device = *app_.device();
  const VkRenderPass& render_pass = *app_.render_pass();
  const Swapchain& swapchain = app_.swapchain();

  const string& vert_code = util::ReadFile(vert_file_);
  const string& frag_code = util::ReadFile(frag_file_);

  VkShaderModule vert_shader_module = CreateShaderModule(device, vert_code);
  VkShaderModule frag_shader_module = CreateShaderModule(device, frag_code);

  VkPipelineShaderStageCreateInfo vert_shader_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .module = vert_shader_module,
    .pName = "main", // entry point of this shader
    // may use .pSpecializationInfo to specify shader constants
  };

  VkPipelineShaderStageCreateInfo frag_shader_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    .module = frag_shader_module,
    .pName = "main", // entry point of this shader
  };

  VkPipelineShaderStageCreateInfo shader_infos[]{
    vert_shader_info,
    frag_shader_info
  };

  // currently pass static data
  auto binding_descs = VertexAttrib::binding_descriptions();
  auto attrib_descs = VertexAttrib::attrib_descriptions();

  VkPipelineVertexInputStateCreateInfo vertex_input_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = CONTAINER_SIZE(binding_descs),
    .pVertexBindingDescriptions = binding_descs.data(),
    .vertexAttributeDescriptionCount = CONTAINER_SIZE(attrib_descs),
    .pVertexAttributeDescriptions = attrib_descs.data(),
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    // .topology can be line, line strp, triangle fan, etc
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    // .primitiveRestartEnable matters for drawing line/triangle strips
    .primitiveRestartEnable = false,
  };

  VkExtent2D target_extent = swapchain.extent();

  VkViewport viewport{
    .x = 0.0f,
    .y = 0.0f,
    .width  = static_cast<float>(target_extent.width),
    .height = static_cast<float>(target_extent.height),
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  VkRect2D scissor{
    .offset = {0, 0},
    .extent = target_extent,
  };

  VkPipelineViewportStateCreateInfo viewport_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .pViewports = &viewport,
    .scissorCount = 1,
    .pScissors = &scissor,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    // fragments beyond clip space will be discarded, not clamped
    .depthClampEnable = VK_FALSE,
    // disable outputs to framebuffer if TRUE
    .rasterizerDiscardEnable = VK_FALSE,
    // fill polygons with fragments
    .polygonMode = VK_POLYGON_MODE_FILL,
    .lineWidth = 1.0f,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    // don't let rasterizer alter depth values
    .depthBiasEnable = VK_FALSE,
  };

  VkPipelineMultisampleStateCreateInfo multisample_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .sampleShadingEnable = VK_FALSE,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };

  // config per attached framebuffer
  VkPipelineColorBlendAttachmentState color_blend_attachment{
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
                    | VK_COLOR_COMPONENT_A_BIT,
    .blendEnable = VK_FALSE,
  };

  // global color blending settings
  VkPipelineColorBlendStateCreateInfo color_blend_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = VK_FALSE,
    .attachmentCount = 1,
    .pAttachments = &color_blend_attachment,
    // may set blend constants here
  };

  // some properties can be modified without recreating entire pipeline
  VkPipelineDynamicStateCreateInfo dynamic_state_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = 0,
  };

  // used to set uniform values
  const auto& descriptor_set_layouts =
      app_.uniform_buffer().descriptor_set_layouts();
  VkPipelineLayoutCreateInfo layout_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = CONTAINER_SIZE(descriptor_set_layouts),
    .pSetLayouts = descriptor_set_layouts.data(),
  };

  ASSERT_SUCCESS(vkCreatePipelineLayout(
                     device, &layout_info, nullptr, &pipeline_layout_),
                 "Failed to create pipeline layout");

  VkGraphicsPipelineCreateInfo pipeline_info{
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = 2,
    .pStages = shader_infos,
    .pVertexInputState = &vertex_input_info,
    .pInputAssemblyState = &input_assembly_info,
    .pViewportState = &viewport_info,
    .pRasterizationState = &rasterizer_info,
    .pMultisampleState = &multisample_info,
    .pDepthStencilState = nullptr,
    .pColorBlendState = &color_blend_info,
    .pDynamicState = &dynamic_state_info,
    .layout = pipeline_layout_,
    .renderPass = render_pass,
    .subpass = 0, // index of subpass where pipeline will be used
    // .basePipeline can be used to copy settings from another piepeline
  };

  ASSERT_SUCCESS(vkCreateGraphicsPipelines(
                     device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                     &pipeline_),
                 "Failed to create graphics pipeline");

  vkDestroyShaderModule(device, vert_shader_module, nullptr);
  vkDestroyShaderModule(device, frag_shader_module, nullptr);
}

void Pipeline::Cleanup() {
  const VkDevice& device = *app_.device();
  vkDestroyPipeline(device, pipeline_, nullptr);
  vkDestroyPipelineLayout(device, pipeline_layout_, nullptr);
}

} /* namespace vulkan */
