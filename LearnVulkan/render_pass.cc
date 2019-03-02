//
//  render_pass.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "render_pass.h"

#include "application.h"

using namespace std;

namespace vulkan {

namespace {

void CreateFramebuffers(vector<VkFramebuffer>* framebuffers,
                        VkExtent2D image_extent,
                        const vector<VkImageView>& image_views,
                        const VkDevice& device,
                        const VkRenderPass& render_pass) {
  framebuffers->resize(image_views.size());
  for (size_t i = 0; i < image_views.size(); ++i) {
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = render_pass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &image_views[i];
    framebufferInfo.width = image_extent.width;
    framebufferInfo.height = image_extent.height;
    framebufferInfo.layers = 1;

    ASSERT_SUCCESS(vkCreateFramebuffer(
                       device, &framebufferInfo, nullptr, &(*framebuffers)[i]),
                   "Failed to create framebuffer");
  }
}

} /* namespace */

void RenderPass::Init() {
  const VkDevice& device = *app_.device();
  const SwapChain& swap_chain = app_.swap_chain();

  VkAttachmentDescription color_att_desc{};
  color_att_desc.format = swap_chain.format();
  color_att_desc.samples = VK_SAMPLE_COUNT_1_BIT; // no multisampling
  // .loadOp and .storeOp affect color and depth buffers
  // .loadOp options: LOAD / CLEAR / DONT_CARE
  // .storeOp options: STORE / DONT_STORE
  color_att_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_att_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_att_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_att_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  // layout of pixels in memory. commonly used options:
  //   - VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: for color attachment
  //   - VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: for images in swap chain
  //   - VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: for images as dstination
  //                                           for memory copy
  //   - VK_IMAGE_LAYOUT_UNDEFINED: don't care about layout before render pass
  color_att_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_att_desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_att_ref{};
  color_att_ref.attachment = 0; // index of attachment to reference to
  color_att_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass_desc{};
  subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  // layout (location = 0) will be rendered to the first attachement
  subpass_desc.colorAttachmentCount = 1;
  subpass_desc.pColorAttachments = &color_att_ref;

  // render pass takes care of layout transition, so it has to wait until
  // image is ready. VK_SUBPASS_EXTERNAL means subpass before (if .srcSubpass)
  // or after (if .dstSubpass) render pass
  VkSubpassDependency subpass_dep{};
  subpass_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpass_dep.dstSubpass = 0; // refer to our subpass
  subpass_dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpass_dep.srcAccessMask = 0;
  subpass_dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpass_dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                            | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &color_att_desc;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass_desc;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &subpass_dep;

  ASSERT_SUCCESS(vkCreateRenderPass(
                     device, &render_pass_info, nullptr, &render_pass_),
                 "Failed to create render pass");

  CreateFramebuffers(&framebuffers_, swap_chain.extent(),
                     swap_chain.image_views(), device, render_pass_);
}

void RenderPass::Cleanup() {
  const VkDevice& device = *app_.device();
  for (const auto& framebuffer : framebuffers_)
    vkDestroyFramebuffer(device, framebuffer, nullptr);
  vkDestroyRenderPass(device, render_pass_, nullptr);
}

} /* namespace vulkan */
