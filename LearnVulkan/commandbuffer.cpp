//
//  commandbuffer.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "commandbuffer.hpp"

#include "application.hpp"
#include "utils.hpp"

namespace VulkanWrappers {
    CommandBuffer::CommandBuffer(const Application &app) : app{app} {
        createCommandObjects();
        recordCommandBuffers();
        createSyncObjects();
    }
    
    void CommandBuffer::createCommandObjects() {
        // create a pool to hold command buffers
        VkCommandPoolCreateInfo commandPoolInfo{};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.queueFamilyIndex = app.getQueues().graphicsFamily;
        
        ASSERT_SUCCESS(vkCreateCommandPool(app.getDevice(), &commandPoolInfo, nullptr, &commandPool),
                       "Failed to create command pool");
        
        // allocate command buffers
        commandBuffers.resize(app.getRenderPass().getFramebuffers().size());
        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = commandPool;
        // secondary level command buffers can be called from primary level
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
        
        ASSERT_SUCCESS(vkAllocateCommandBuffers(app.getDevice(), &cmdAllocInfo, commandBuffers.data()),
                       "Failed to allocate command buffers");
    }
    
    void CommandBuffer::recordCommandBuffers() {
        for (size_t i = 0; i < commandBuffers.size(); ++i) {
            // start command buffer recording
            VkCommandBufferBeginInfo cmdBeginInfo{};
            cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
            // .pInheritanceInfo sets what to inherit from primary buffers to secondary buffers
            
            ASSERT_SUCCESS(vkBeginCommandBuffer(commandBuffers[i], &cmdBeginInfo),
                           "Failed to begin recording command buffer");
            
            // start render pass
            VkRenderPassBeginInfo rpBeginInfo{};
            rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpBeginInfo.renderPass = *app.getRenderPass();
            rpBeginInfo.framebuffer = app.getRenderPass().getFramebuffers()[i];
            rpBeginInfo.renderArea.offset = {0, 0};
            rpBeginInfo.renderArea.extent = app.getSwapChain().getExtent();
            VkClearValue clearColor{0.0f, 0.0f, 0.0f, 1.0f};
            rpBeginInfo.clearValueCount = 1;
            rpBeginInfo.pClearValues = &clearColor; // used for VK_ATTACHMENT_LOAD_OP_CLEAR
            
            // record commends. options:
            //   - VK_SUBPASS_CONTENTS_INLINE: use primary commmand buffer
            //   - VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: use secondary
            vkCmdBeginRenderPass(commandBuffers[i], &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, *app.getPipeline());
            vkCmdDraw(commandBuffers[i], 3, 1, 0, 0); // (vertexCount, instanceCount, firstVertex, firstInstance)
            vkCmdEndRenderPass(commandBuffers[i]);
            
            // end recording
            ASSERT_SUCCESS(vkEndCommandBuffer(commandBuffers[i]),
                           "Failed to end recording command buffer");
        }
    }
    
    void CommandBuffer::createSyncObjects() {
        // used to sync draw commands and presentation
        imageAvailableSemas.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemas.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            ASSERT_SUCCESS(vkCreateSemaphore(app.getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemas[i]),
                           "Failed to create image available semaphore");
            ASSERT_SUCCESS(vkCreateSemaphore(app.getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemas[i]),
                           "Failed to create render finished semaphore");
            ASSERT_SUCCESS(vkCreateFence(app.getDevice(), &fenceInfo, nullptr, &inFlightFences[i]),
                           "Failed to create in flight fence");
        }
    }
    
    void CommandBuffer::drawFrame() {
        // fence was initialized to signaled state, so that waiting for it at the beginning is fine
        vkWaitForFences(app.getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, numeric_limits<uint64_t>::max());
        vkResetFences(app.getDevice(), 1, &inFlightFences[currentFrame]); // manually resetting, unlike semophore
        
        uint32_t imageIndex;
        vkAcquireNextImageKHR(app.getDevice(), *app.getSwapChain(), numeric_limits<uint64_t>::max(),
                              imageAvailableSemas[currentFrame], VK_NULL_HANDLE, &imageIndex);
        
        // wait for image available
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemas[]{imageAvailableSemas[currentFrame]};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemas;
        // we have to wait only if we want to write to color attachment
        // so we actually can start running pipeline long before that image is ready
        // we specify one stage for each semaphore, so it doesn't need a separate count
        VkPipelineStageFlags waitStages[]{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
        VkSemaphore signalSemas[]{renderFinishedSemas[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemas; // will be signaled once command buffer finishes
        
        ASSERT_SUCCESS(vkQueueSubmit(app.getQueues().graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]),
                       "Failed to submit draw command buffer");
        
        // present image to screen
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemas;
        VkSwapchainKHR swapChains[]{*app.getSwapChain()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex; // image for each swap chain
        // may use .pResults to check wether each swap chain rendered successfully
        
        vkQueuePresentKHR(app.getQueues().presentQueue, &presentInfo);
        
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    
    CommandBuffer::~CommandBuffer() {
        vkDestroyCommandPool(app.getDevice(), commandPool, nullptr);
        // command buffers are implicitly cleaned up with command pool
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(app.getDevice(), imageAvailableSemas[i], nullptr);
            vkDestroySemaphore(app.getDevice(), renderFinishedSemas[i], nullptr);
            vkDestroyFence(app.getDevice(), inFlightFences[i], nullptr);
        }
    }
}
