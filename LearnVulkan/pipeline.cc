//
//  pipeline.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 2/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "pipeline.h"

#include "application.h"
#include "util.h"
#include "vertex_buffer.h"

namespace vulkan {
    namespace {
        VkShaderModule createShaderModule(const Device &device,
                                          const string &code) {
            VkShaderModuleCreateInfo shaderModuleInfo{};
            shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleInfo.codeSize = code.length();
            shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
            
            VkShaderModule shaderModule{};
            ASSERT_SUCCESS(vkCreateShaderModule(*device, &shaderModuleInfo, nullptr, &shaderModule),
                           "Failed to create shader module");
            
            return shaderModule;
        }
    }
    
    void Pipeline::init() {
        const string &vertCode = util::readFile(vertFile);
        const string &fragCode = util::readFile(fragFile);
        
        VkShaderModule vertShaderModule = createShaderModule(app.device(), vertCode);
        VkShaderModule fragShaderModule = createShaderModule(app.device(), fragCode);
        
        VkPipelineShaderStageCreateInfo vertShaderInfo{};
        vertShaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderInfo.module = vertShaderModule;
        vertShaderInfo.pName = "main"; // entry point of this shader
        // may use .pSpecializationInfo to specify shader constants
        
        VkPipelineShaderStageCreateInfo fragShaderInfo{};
        fragShaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderInfo.module = fragShaderModule;
        fragShaderInfo.pName = "main"; // entry point of this shader
        
        VkPipelineShaderStageCreateInfo shaderStages[]{
            vertShaderInfo,
            fragShaderInfo,
        };
        
        // currently pass static data
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        
        auto bindingDescs = Vertex::getBindingDescriptions();
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescs.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescs.data();
        
        auto attributeDescs = Vertex::getAttributeDescriptions();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescs.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescs.data();
        
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
        inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // line, line strp, triangle fan, etc
        inputAssemblyInfo.primitiveRestartEnable = false; // matters for drawing line/triangle strips
        
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        VkExtent2D targetExtent = app.swap_chain().getExtent();
        viewport.width  = targetExtent.width;
        viewport.height = targetExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = targetExtent;
        
        VkPipelineViewportStateCreateInfo viewportInfo{};
        viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportInfo.viewportCount = 1;
        viewportInfo.pViewports = &viewport;
        viewportInfo.scissorCount = 1;
        viewportInfo.pScissors = &scissor;
        
        VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
        rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizerInfo.depthClampEnable = VK_FALSE; // fragments beyond clip space will be discarded, not clamped
        rasterizerInfo.rasterizerDiscardEnable = VK_FALSE; // disable outputs to framebuffer if TRUE
        rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL; // fill polygons with fragments
        rasterizerInfo.lineWidth = 1.0f;
        rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizerInfo.depthBiasEnable = VK_FALSE; // don't let rasterizer alter depth values
        
        VkPipelineMultisampleStateCreateInfo multisampleInfo{};
        multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleInfo.sampleShadingEnable = VK_FALSE;
        multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        
        VkPipelineColorBlendAttachmentState colorBlendAttachment{}; // config per attached framebuffer
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        
        VkPipelineColorBlendStateCreateInfo colorBlendInfo{}; // global color blending settings
        colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendInfo.logicOpEnable = VK_FALSE;
        colorBlendInfo.attachmentCount = 1;
        colorBlendInfo.pAttachments = &colorBlendAttachment;
        // set blend constants here
        
        // some properties can be modified without recreating entire pipeline
        VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
        dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateInfo.dynamicStateCount = 0;
        
        // used to set uniform values
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        
        ASSERT_SUCCESS(vkCreatePipelineLayout(*app.device(), &layoutInfo, nullptr, &layout),
                       "Failed to create pipeline layout");
        
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
        pipelineInfo.pViewportState = &viewportInfo;
        pipelineInfo.pRasterizationState = &rasterizerInfo;
        pipelineInfo.pMultisampleState = &multisampleInfo;
        pipelineInfo.pDepthStencilState = nullptr;
        pipelineInfo.pColorBlendState = &colorBlendInfo;
        pipelineInfo.pDynamicState = &dynamicStateInfo;
        pipelineInfo.layout = layout;
        pipelineInfo.renderPass = *app.render_pass();
        pipelineInfo.subpass = 0; // index of subpass where this pipeline will be used
        // .basePipeline{Handle, Index} can be used to copy settings from another piepeline
        
        ASSERT_SUCCESS(vkCreateGraphicsPipelines(*app.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline),
                       "Failed to create graphics pipeline");
        
        vkDestroyShaderModule(*app.device(), vertShaderModule, nullptr);
        vkDestroyShaderModule(*app.device(), fragShaderModule, nullptr);
    }
    
    void Pipeline::cleanup() {
        vkDestroyPipeline(*app.device(), pipeline, nullptr);
        vkDestroyPipelineLayout(*app.device(), layout, nullptr);
    }
    
    Pipeline::~Pipeline() {
        cleanup();
    }
}
