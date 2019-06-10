//
//  render_pass.cc
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/render_pass.h"

#include "absl/memory/memory.h"
#include "jessie_steamer/wrapper/vulkan/macro.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

const VkClearValue& GetClearColor() {
  static VkClearValue* kClearColor = nullptr;
  if (kClearColor == nullptr) {
    kClearColor = new VkClearValue{};
    float* clear_color = kClearColor->color.float32;
    clear_color[0] = 0.0f;
    clear_color[1] = 0.0f;
    clear_color[2] = 0.0f;
    clear_color[3] = 1.0f;
  }
  return *kClearColor;
}

const VkClearValue& GetClearDepthStencil() {
  static VkClearValue* kClearDepthStencil = nullptr;
  if (kClearDepthStencil == nullptr) {
    kClearDepthStencil = new VkClearValue{};
    kClearDepthStencil->depthStencil = {/*depth=*/1.0f, /*stencil=*/0};
  }
  return *kClearDepthStencil;
}

std::vector<VkFramebuffer> CreateFramebuffers(
    const SharedBasicContext& context,
    const Swapchain& swapchain,
    const DepthStencilImage& depth_stencil_image,
    const VkRenderPass& render_pass) {
  // although we have multiple swapchain images, we will share one depth stencil
  // image, because we only use one graphics queue, which only renders on one
  // swapchain image at a time
  std::vector<VkFramebuffer> framebuffers(swapchain.size());
  for (int i = 0; i < swapchain.size(); ++i) {
    std::array<VkImageView, 2> attachments{
        swapchain.image_view(i),
        depth_stencil_image.image_view(),
    };

    VkFramebufferCreateInfo framebuffer_info{
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        /*pNext=*/nullptr,
        /*flags=*/nullflag,
        render_pass,
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

RenderPassBuilder RenderPassBuilder::DefaultBuilder(
    SharedBasicContext context,
    const Swapchain& swapchain,
    const DepthStencilImage& depth_stencil_image) {
  RenderPassBuilder builder{std::move(context)};

  builder.attachment_descriptions_.push_back({  // color
      /*flags=*/nullflag,
      swapchain.format(),
      /*samples=*/VK_SAMPLE_COUNT_1_BIT,  // no multi-sampling
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
      //   - VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: for images as destination
      //                                           for memory copy
      //   - VK_IMAGE_LAYOUT_UNDEFINED: don't care about layout before this
      //                                render pass
      /*initialLayout=*/VK_IMAGE_LAYOUT_UNDEFINED,
      /*finalLayout=*/VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  });
  builder.attachment_descriptions_.push_back({  // depth stencil
      /*flags=*/nullflag,
      depth_stencil_image.format(),
      /*samples=*/VK_SAMPLE_COUNT_1_BIT,  // no multi-sampling
      /*loadOp=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
      /*storeOp=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
      /*stencilLoadOp=*/VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      /*stencilStoreOp=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
      /*initialLayout=*/VK_IMAGE_LAYOUT_UNDEFINED,
      /*finalLayout=*/VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  });

  builder.attachment_references_.push_back({  // color
      /*attachment=*/0,  // index of attachment to reference to
      /*layout=*/VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  });
  builder.attachment_references_.push_back({  // depth stencil
      /*attachment=*/1,
      /*layout=*/VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  });

  builder.subpass_descriptions_.push_back({
      /*flags=*/nullflag,
      /*pipelineBindPoint=*/VK_PIPELINE_BIND_POINT_GRAPHICS,
      /*inputAttachmentCount=*/0,
      /*pInputAttachments=*/nullptr,
      // layout (location = 0) will be rendered to the first attachment
      /*colorAttachmentCount=*/1,
      &builder.attachment_references_[0],
      /*pResolveAttachments=*/nullptr,
      // a render pass can only use one depth stencil buffer, so no count needed
      &builder.attachment_references_[1],
      /*preserveAttachmentCount=*/0,
      /*pPreserveAttachments=*/nullptr,
  });

  // render pass takes care of layout transition, so it has to wait until
  // image is ready. VK_SUBPASS_EXTERNAL means subpass before (if .srcSubpass)
  // or after (if .dstSubpass) render pass
  builder.subpass_dependencies_.push_back({
      /*srcSubpass=*/VK_SUBPASS_EXTERNAL,
      /*dstSubpass=*/0,  // refer to our subpass
      /*srcStageMask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      /*dstStageMask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      /*srcAccessMask=*/0,
      /*dstAccessMask=*/VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                            | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      /*dependencyFlags=*/nullflag,
  });

  builder.clear_values_ = {GetClearColor(), GetClearDepthStencil()};

  return builder;
}

std::unique_ptr<RenderPass> RenderPassBuilder::Build(
    const Swapchain& swapchain,
    const DepthStencilImage& depth_stencil_image) {
  VkRenderPassCreateInfo render_pass_info{
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      CONTAINER_SIZE(attachment_descriptions_),
      attachment_descriptions_.data(),
      CONTAINER_SIZE(subpass_descriptions_),
      subpass_descriptions_.data(),
      CONTAINER_SIZE(subpass_dependencies_),
      subpass_dependencies_.data(),
  };

  VkRenderPass render_pass;
  ASSERT_SUCCESS(vkCreateRenderPass(*context_->device(), &render_pass_info,
                                    context_->allocator(), &render_pass),
                 "Failed to create render pass");

  return absl::make_unique<RenderPass>(
      context_, render_pass, std::move(clear_values_), CreateFramebuffers(
          context_, swapchain, depth_stencil_image, render_pass));
}

void RenderPass::Run(const VkCommandBuffer& command_buffer,
                     int framebuffer_index,
                     VkExtent2D frame_size,
                     const RenderOps& render_ops) const {
  VkRenderPassBeginInfo begin_info{
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      /*pNext=*/nullptr,
      render_pass_,
      framebuffers_[framebuffer_index],
      /*renderArea=*/{
          /*offset=*/{0, 0},
          frame_size,
      },
      CONTAINER_SIZE(clear_values_),
      clear_values_.data(),  // used for _OP_CLEAR
  };

  // record commends. options:
  //   - VK_SUBPASS_CONTENTS_INLINE: use primary command buffer
  //   - VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: use secondary
  vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
  render_ops();
  vkCmdEndRenderPass(command_buffer);
}

RenderPass::~RenderPass() {
  for (const auto& framebuffer : framebuffers_) {
    vkDestroyFramebuffer(*context_->device(), framebuffer,
                         context_->allocator());
  }
  vkDestroyRenderPass(*context_->device(), render_pass_, context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
