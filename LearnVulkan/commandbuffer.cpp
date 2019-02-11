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
    void createCommandPool(VkCommandPool&, uint32_t, const Device&);
    void createCommandBuffers(vector<VkCommandBuffer>&, size_t,
                              const Device&, const VkCommandPool&);
    void recordCommands(const vector<VkCommandBuffer>&, const RenderPass&,
                        const SwapChain&, const Pipeline&);
    void createSemaphore(vector<VkSemaphore>&, size_t, const Device&);
    void createFence(vector<VkFence>&, size_t, const Device&);
    
    void CommandBuffer::createSyncObjects() {
        createSemaphore(imageAvailableSemas, MAX_FRAMES_IN_FLIGHT, app.getDevice());
        createSemaphore(renderFinishedSemas, MAX_FRAMES_IN_FLIGHT, app.getDevice());
        createFence(inFlightFences, MAX_FRAMES_IN_FLIGHT, app.getDevice());
    }
    
    VkResult CommandBuffer::drawFrame() {
        // fence was initialized to signaled state, so that waiting for it at the beginning is fine
        vkWaitForFences(*app.getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, numeric_limits<uint64_t>::max());
        
        // acquire swap chain image
        uint32_t imageIndex;
        VkResult acquireResult = vkAcquireNextImageKHR(*app.getDevice(), *app.getSwapChain(), numeric_limits<uint64_t>::max(),
                                                       imageAvailableSemas[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) // triggered when swap chain can no longer present image
            return acquireResult;
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
            throw runtime_error{"Failed to acquire swap chain image"};
        
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
        
        vkResetFences(*app.getDevice(), 1, &inFlightFences[currentFrame]); // reset to unsignaled state
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
        
        VkResult presentResult = vkQueuePresentKHR(app.getQueues().presentQueue, &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) // triggered when swap chain can no longer present image
            return presentResult;
        if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR)
            throw runtime_error{"Failed to present swap chain image"};
        
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        return VK_SUCCESS;
    }
    
    void CommandBuffer::init() {
        if (firstTime) {
            createCommandPool(commandPool, app.getQueues().graphicsFamily, app.getDevice());
            createSyncObjects();
            firstTime = false;
        }
        createCommandBuffers(commandBuffers, app.getRenderPass().getFramebuffers().size(),
                             app.getDevice(), commandPool);
        recordCommands(commandBuffers, app.getRenderPass(), app.getSwapChain(), app.getPipeline());
    }
    
    void CommandBuffer::cleanup() {
        vkFreeCommandBuffers(*app.getDevice(), commandPool,
                             static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    }
    
    CommandBuffer::~CommandBuffer() {
        vkDestroyCommandPool(*app.getDevice(), commandPool, nullptr);
        // command buffers are implicitly cleaned up with command pool
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(*app.getDevice(), imageAvailableSemas[i], nullptr);
            vkDestroySemaphore(*app.getDevice(), renderFinishedSemas[i], nullptr);
            vkDestroyFence(*app.getDevice(), inFlightFences[i], nullptr);
        }
    }
    
    void createCommandPool(VkCommandPool &pool,
                           uint32_t queueIdx,
                           const Device &device) {
        // create a pool to hold command buffers
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueIdx;
        
        ASSERT_SUCCESS(vkCreateCommandPool(*device, &poolInfo, nullptr, &pool),
                       "Failed to create command pool");
    }
    
    void createCommandBuffers(vector<VkCommandBuffer> &buffers,
                              size_t count,
                              const Device &device,
                              const VkCommandPool &pool) {
        // allocate command buffers
        buffers.resize(count);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = pool;
        // secondary level command buffers can be called from primary level
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(count);
        
        ASSERT_SUCCESS(vkAllocateCommandBuffers(*device, &allocInfo, buffers.data()),
                       "Failed to allocate command buffers");
    }
    
    void recordCommands(const vector<VkCommandBuffer> &buffers,
                        const RenderPass &renderPass,
                        const SwapChain &swapChain,
                        const Pipeline &pipeline) {
        for (size_t i = 0; i < buffers.size(); ++i) {
            // start command buffer recording
            VkCommandBufferBeginInfo cmdBeginInfo{};
            cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
            // .pInheritanceInfo sets what to inherit from primary buffers to secondary buffers
            
            ASSERT_SUCCESS(vkBeginCommandBuffer(buffers[i], &cmdBeginInfo),
                           "Failed to begin recording command buffer");
            
            // start render pass
            VkRenderPassBeginInfo rpBeginInfo{};
            rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpBeginInfo.renderPass = *renderPass;
            rpBeginInfo.framebuffer = renderPass.getFramebuffers()[i];
            rpBeginInfo.renderArea.offset = {0, 0};
            rpBeginInfo.renderArea.extent = swapChain.getExtent();
            VkClearValue clearColor{0.0f, 0.0f, 0.0f, 1.0f};
            rpBeginInfo.clearValueCount = 1;
            rpBeginInfo.pClearValues = &clearColor; // used for VK_ATTACHMENT_LOAD_OP_CLEAR
            
            // record commends. options:
            //   - VK_SUBPASS_CONTENTS_INLINE: use primary commmand buffer
            //   - VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: use secondary
            vkCmdBeginRenderPass(buffers[i], &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
            vkCmdDraw(buffers[i], 3, 1, 0, 0); // (vertexCount, instanceCount, firstVertex, firstInstance)
            vkCmdEndRenderPass(buffers[i]);
            
            // end recording
            ASSERT_SUCCESS(vkEndCommandBuffer(buffers[i]),
                           "Failed to end recording command buffer");
        }
    }
    
    void createSemaphore(vector<VkSemaphore> &semas,
                         size_t count,
                         const Device &device) {
        VkSemaphoreCreateInfo semaInfo{};
        semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        semas.resize(count);
        for (auto &sema : semas) {
            ASSERT_SUCCESS(vkCreateSemaphore(*device, &semaInfo, nullptr, &sema),
                           "Failed to create semaphore");
        }
    }
    
    void createFence(vector<VkFence> &fences,
                     size_t count,
                     const Device &device) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        
        fences.resize(count);
        for (auto &fence : fences) {
            ASSERT_SUCCESS(vkCreateFence(*device, &fenceInfo, nullptr, &fence),
                           "Failed to create in flight fence");
        }
    }
}
