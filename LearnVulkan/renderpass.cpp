//
//  renderpass.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "renderpass.hpp"

#include "application.hpp"
#include "utils.hpp"

namespace VulkanWrappers {
    RenderPass::RenderPass(const Application &app) : app{app} {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = app.getSwapChain().getFormat();
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // no multisampling
        // loadOp and storeOp affect color and depth buffers
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // LOAD / CLEAR / DONT_CARE
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // STORE / DONT_STORE
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // layout of pixels in memory. other commonly used ones:
        //   - VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: for color attachment
        //   - VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: for images as dstination for memory copy
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // don't care about layout before render pass
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // for images in swap chain
        
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0; // index of attachment to reference to
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // layout used in subpass
        
        VkSubpassDescription subpassDesc{};
        subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDesc.colorAttachmentCount = 1; // layout (location = 0) will be rendered to the first attachement
        subpassDesc.pColorAttachments = &colorAttachmentRef;
        
        // render pass takes scre of layout transition, so it has to wait until image is ready
        // VK_SUBPASS_EXTERNAL means subpass before (if .srcSubpass) / after (if .dstSubpass) render pass
        VkSubpassDependency subpassDep{};
        subpassDep.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpassDep.dstSubpass = 0; // refer to our subpass
        subpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDep.srcAccessMask = 0;
        subpassDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpassDesc;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &subpassDep;
        
        ASSERT_TRUE(vkCreateRenderPass(app.getDevice(), &renderPassInfo, nullptr, &renderPass),
                    "Failed to create render pass");
        
        const auto &imageViews = app.getSwapChain().getImageViews();
        const auto &imageExtent = app.getSwapChain().getExtent();
        framebuffers.resize(imageViews.size());
        for (size_t i = 0; i < imageViews.size(); ++i) {
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = &imageViews[i];
            framebufferInfo.width = imageExtent.width;
            framebufferInfo.height = imageExtent.height;
            framebufferInfo.layers = 1;
            
            ASSERT_TRUE(vkCreateFramebuffer(app.getDevice(), &framebufferInfo, nullptr, &framebuffers[i]),
                        "Failed to create framebuffer");
        }
    }
    
    RenderPass::~RenderPass() {
        for (const auto &framebuffer : framebuffers)
            vkDestroyFramebuffer(app.getDevice(), framebuffer, nullptr);
        vkDestroyRenderPass(app.getDevice(), renderPass, nullptr);
    }
}
