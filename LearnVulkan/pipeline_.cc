//
//  pipeline.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 2/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "pipeline_.h"

#include "application.h"
#include "util.h"
#include "vertex_buffer.h"

namespace vulkan {

namespace {

VkShaderModule CreateShaderModule(const VkDevice& device,
                                  const string& code) {
    VkShaderModuleCreateInfo shader_module_info{};
    shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_info.codeSize = code.length();
    shader_module_info.pCode =
        reinterpret_cast<const uint32_t*>(code.data());
    
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
    const SwapChain& swap_chain = app_.swap_chain();
    
    const string& vert_code = util::ReadFile(vert_file_);
    const string& frag_code = util::ReadFile(frag_file_);
    
    VkShaderModule vert_shader_module = CreateShaderModule(device, vert_code);
    VkShaderModule frag_shader_module = CreateShaderModule(device, frag_code);
    
    VkPipelineShaderStageCreateInfo vert_shader_info{};
    vert_shader_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_info.module = vert_shader_module;
    vert_shader_info.pName = "main"; // entry point of this shader
    // may use .pSpecializationInfo to specify shader constants
    
    VkPipelineShaderStageCreateInfo frag_shader_info{};
    frag_shader_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_info.module = frag_shader_module;
    frag_shader_info.pName = "main"; // entry point of this shader
    
    VkPipelineShaderStageCreateInfo shader_infos[]{
        vert_shader_info,
        frag_shader_info,
    };
    
    // currently pass static data
    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    
    auto binding_descs = VertexAttrib::binding_descriptions();
    vertex_input_info.vertexBindingDescriptionCount =
        CONTAINER_SIZE(binding_descs);
    vertex_input_info.pVertexBindingDescriptions = binding_descs.data();
    
    auto attrib_descs = VertexAttrib::attrib_descriptions();
    vertex_input_info.vertexAttributeDescriptionCount =
        CONTAINER_SIZE(attrib_descs);
    vertex_input_info.pVertexAttributeDescriptions = attrib_descs.data();
    
    VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
    input_assembly_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    // .topology can be line, line strp, triangle fan, etc
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // .primitiveRestartEnable matters for drawing line/triangle strips
    input_assembly_info.primitiveRestartEnable = false;
    
    VkExtent2D target_extent = swap_chain.extent();
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width  = target_extent.width;
    viewport.height = target_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = target_extent;
    
    VkPipelineViewportStateCreateInfo viewport_info{};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.pViewports = &viewport;
    viewport_info.scissorCount = 1;
    viewport_info.pScissors = &scissor;
    
    VkPipelineRasterizationStateCreateInfo rasterizer_info{};
    rasterizer_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    // fragments beyond clip space will be discarded, not clamped
    rasterizer_info.depthClampEnable = VK_FALSE;
    // disable outputs to framebuffer if TRUE
    rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
    // fill polygons with fragments
    rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer_info.lineWidth = 1.0f;
    rasterizer_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    // don't let rasterizer alter depth values
    rasterizer_info.depthBiasEnable = VK_FALSE;
    
    VkPipelineMultisampleStateCreateInfo multisample_info{};
    multisample_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_info.sampleShadingEnable = VK_FALSE;
    multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // config per attached framebuffer
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                                          | VK_COLOR_COMPONENT_G_BIT
                                          | VK_COLOR_COMPONENT_B_BIT
                                          | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;
    
    // global color blending settings
    VkPipelineColorBlendStateCreateInfo color_blend_info{};
    color_blend_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_info.logicOpEnable = VK_FALSE;
    color_blend_info.attachmentCount = 1;
    color_blend_info.pAttachments = &color_blend_attachment;
    // may set blend constants here
    
    // some properties can be modified without recreating entire pipeline
    VkPipelineDynamicStateCreateInfo dynamic_state_info{};
    dynamic_state_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = 0;
    
    // used to set uniform values
    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    
    ASSERT_SUCCESS(vkCreatePipelineLayout(
                       device, &layout_info, nullptr, &layout_),
                   "Failed to create pipeline layout");
    
    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_infos;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pViewportState = &viewport_info;
    pipeline_info.pRasterizationState = &rasterizer_info;
    pipeline_info.pMultisampleState = &multisample_info;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blend_info;
    pipeline_info.pDynamicState = &dynamic_state_info;
    pipeline_info.layout = layout_;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0; // index of subpass where pipeline will be used
    // .basePipeline can be used to copy settings from another piepeline
    
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
    vkDestroyPipelineLayout(device, layout_, nullptr);
}

} /* namespace vulkan */
