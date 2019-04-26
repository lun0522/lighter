//
//  render_pass.cc
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/render_pass.h"

#include <array>

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/context.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using common::util::nullflag;
using std::vector;

vector<VkFramebuffer> CreateFramebuffers(
    const SharedContext& context,
    const DepthStencilImage& depth_stencil_image) {
  const Swapchain& swapchain = context->swapchain();

  // although we have multiple swapchain images, we will share one depth stencil
  // image, because we only use one graphics queue, which only renders on one
  // swapchain image at a time
  vector<VkFramebuffer> framebuffers(swapchain.size());
  for (size_t i = 0; i < swapchain.size(); ++i) {
    std::array<VkImageView, 2> attachments{
        swapchain.image_view(i),
        depth_stencil_image.image_view(),
    };

    VkFramebufferCreateInfo framebuffer_info{
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        /*pNext=*/nullptr,
        nullflag,
        *context->render_pass(),
        CONTAINER_SIZE(attachments),
        attachments.data(),
        swapchain.extent().width,
        swapchain.extent().height,
        /*layers=*/1,
    };

    ASSERT_SUCCESS(vkCreateFramebuffer(*context->device(), &framebuffer_info,
                                       context->allocator(), &framebuffers[i]),
                   "Failed to create framebuffer");
  }
  return framebuffers;
}

} /* namespace */

void RenderPass::Init(SharedContext context) {
  context_ = std::move(context);
  depth_stencil_.Init(context_, context_->swapchain().extent());

  VkAttachmentDescription color_att_desc{
      nullflag,
      context_->swapchain().format(),
      /*samples=*/VK_SAMPLE_COUNT_1_BIT,  // no multisampling
      // .loadOp and .storeOp affect color and depth buffers
      // .loadOp options: LOAD / CLEAR / DONT_CARE
      // .storeOp options: STORE / DONT_STORE
      /*loadOp=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
      /*storeOp=*/VK_ATTACHMENT_STORE_OP_STORE,
      /*stencilLoadOp=*/VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      /*stencilStoreOp=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
      // layout of pixels in memory. commonly used options:
      //   - VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: for color attachment
      //   - VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: for images in swapchain
      //   - VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: for images as dstination
      //                                           for memory copy
      //   - VK_IMAGE_LAYOUT_UNDEFINED: don't care about layout before this
      //                                render pass
      /*initialLayout=*/VK_IMAGE_LAYOUT_UNDEFINED,
      /*finalLayout=*/VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference color_att_ref{
      /*attachment=*/0,  // index of attachment to reference to
      /*layout=*/VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkAttachmentDescription depth_stencil_att_desc{
      nullflag,
      depth_stencil_.format(),
      /*samples=*/VK_SAMPLE_COUNT_1_BIT,  // no multisampling
      /*loadOp=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
      /*storeOp=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
      /*stencilLoadOp=*/VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      /*stencilStoreOp=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
      /*initialLayout=*/VK_IMAGE_LAYOUT_UNDEFINED,
      /*finalLayout=*/VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  VkAttachmentReference depth_stencil_att_ref{
      /*attachment=*/1,
      /*layout=*/VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass_desc{
      nullflag,
      /*pipelineBindPoint=*/VK_PIPELINE_BIND_POINT_GRAPHICS,
      /*inputAttachmentCount=*/0,
      /*pInputAttachments=*/nullptr,
      // layout (location = 0) will be rendered to the first attachement
      /*colorAttachmentCount=*/1,
      &color_att_ref,
      /*pResolveAttachments=*/nullptr,
      // a render pass can only use one depth stencil buffer, so no count needed
      &depth_stencil_att_ref,
      /*preserveAttachmentCount=*/0,
      /*pPreserveAttachments=*/nullptr,
  };

  // render pass takes care of layout transition, so it has to wait until
  // image is ready. VK_SUBPASS_EXTERNAL means subpass before (if .srcSubpass)
  // or after (if .dstSubpass) render pass
  VkSubpassDependency subpass_dep{
      /*srcSubpass=*/VK_SUBPASS_EXTERNAL,
      /*dstSubpass=*/0,  // refer to our subpass
      /*srcStageMask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      /*dstStageMask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      /*srcAccessMask=*/0,
      /*dstAccessMask=*/VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                            | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      /*dependencyFlags=*/nullflag,
  };

  std::array<VkAttachmentDescription, 2> att_descs{
    color_att_desc,
    depth_stencil_att_desc,
  };

  VkRenderPassCreateInfo render_pass_info{
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      /*pNext=*/nullptr,
      nullflag,
      CONTAINER_SIZE(att_descs),
      att_descs.data(),
      /*subpassCount=*/1,
      &subpass_desc,
      /*dependencyCount=*/1,
      &subpass_dep,
  };

  ASSERT_SUCCESS(vkCreateRenderPass(*context_->device(), &render_pass_info,
                                    context_->allocator(), &render_pass_),
                 "Failed to create render pass");

  framebuffers_ = CreateFramebuffers(context_, depth_stencil_);
}

void RenderPass::Cleanup() {
  for (const auto& framebuffer : framebuffers_) {
    vkDestroyFramebuffer(*context_->device(), framebuffer,
                         context_->allocator());
  }
  vkDestroyRenderPass(*context_->device(), render_pass_, context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
