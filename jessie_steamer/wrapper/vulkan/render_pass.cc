//
//  render_pass.cc
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/render_pass.h"

#include "absl/memory/memory.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/macro.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using ColorOps = RenderPassBuilder::Attachment::ColorOps;
using DepthStencilOps = RenderPassBuilder::Attachment::DepthStencilOps;
using std::vector;

vector<VkAttachmentDescription> CreateAttachmentDescriptions(
    const vector<RenderPassBuilder::Attachment>& attachments) {
  vector<VkAttachmentDescription> descriptions(attachments.size());
  for (int i = 0; i < attachments.size(); ++i) {
    auto& description = descriptions[i];
    const auto& attachment = attachments[i];
    description = VkAttachmentDescription{
        /*flags=*/nullflag,
        attachment.get_image(0).format(),
        /*samples=*/VK_SAMPLE_COUNT_1_BIT,  // no multi-sampling
        /*loadOp=*/VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // to be updated
        /*storeOp=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,  // to be updated
        /*stencilLoadOp=*/VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // to be updated
        /*stencilStoreOp=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,  // to be updated
        /*initialLayout=*/attachment.initial_layout,
        /*finalLayout=*/attachment.final_layout,
    };
    if (absl::holds_alternative<ColorOps>(attachment.attachment_ops)) {
      const auto& color_ops = absl::get<ColorOps>(attachment.attachment_ops);
      description.loadOp = color_ops.load_color;
      description.storeOp = color_ops.store_color;
    } else if (absl::holds_alternative<DepthStencilOps>(
        attachment.attachment_ops)) {
      const auto& depth_stencil_ops =
          absl::get<DepthStencilOps>(attachment.attachment_ops);
      description.loadOp = depth_stencil_ops.load_depth;
      description.storeOp = depth_stencil_ops.store_depth;
      description.stencilLoadOp = depth_stencil_ops.load_stencil;
      description.stencilStoreOp = depth_stencil_ops.store_stencil;
    } else {
      throw std::runtime_error{"Unrecognized variant type"};
    }
  }
  return descriptions;
}

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

vector<VkClearValue> CreateClearValues(
    const vector<RenderPassBuilder::Attachment>& attachments) {
  vector<VkClearValue> clear_values;
  clear_values.reserve(attachments.size());
  for (const auto& attachment : attachments) {
    if (absl::holds_alternative<ColorOps>(attachment.attachment_ops)) {
      clear_values.emplace_back(GetClearColor());
    } else if (absl::holds_alternative<DepthStencilOps>(
        attachment.attachment_ops)) {
      clear_values.emplace_back(GetClearDepthStencil());
    } else {
      throw std::runtime_error{"Unrecognized variant type"};
    }
  }
  return clear_values;
}

vector<VkFramebuffer> CreateFramebuffers(
    const SharedBasicContext& context,
    const VkRenderPass& render_pass,
    const vector<RenderPassBuilder::Attachment>& attachments,
    VkExtent2D framebuffer_size, int num_framebuffer) {
  vector<VkFramebuffer> framebuffers(num_framebuffer);
  for (int i = 0; i < framebuffers.size(); ++i) {
    vector<VkImageView> image_views;
    image_views.reserve(attachments.size());
    for (const auto& attachment : attachments) {
      image_views.emplace_back(attachment.get_image(i).image_view());
    }

    VkFramebufferCreateInfo framebuffer_info{
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        /*pNext=*/nullptr,
        /*flags=*/nullflag,
        render_pass,
        CONTAINER_SIZE(image_views),
        image_views.data(),
        framebuffer_size.width,
        framebuffer_size.height,
        /*layers=*/1,
    };

    ASSERT_SUCCESS(vkCreateFramebuffer(*context->device(), &framebuffer_info,
                                       context->allocator(), &framebuffers[i]),
                   "Failed to create framebuffer");
  }
  return framebuffers;
}

} /* namespace */

std::unique_ptr<RenderPassBuilder> RenderPassBuilder::SimpleRenderPassBuilder(
    SharedBasicContext context,
    const Swapchain& swapchain,
    const DepthStencilImage& depth_stencil_image) {
  std::unique_ptr<RenderPassBuilder> builder =
      absl::make_unique<RenderPassBuilder>(std::move(context));

  // number of frame
  builder->set_num_framebuffer(swapchain.num_image());

  // color attachment
  builder->set_attachment(/*index=*/0, Attachment{
      /*attachment_ops=*/Attachment::ColorOps{
          /*load_color=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
          /*store_color=*/VK_ATTACHMENT_STORE_OP_STORE,
      },
      /*get_image=*/[&swapchain](int index) -> const Image& {
        return swapchain.image(index);
      },
      /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
      /*final_layout=*/VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  });

  // depth stencil attachment
  builder->set_attachment(/*index=*/1, Attachment{
      /*attachment_ops=*/Attachment::DepthStencilOps{
          /*load_depth=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
          /*store_depth=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
          /*load_stencil=*/VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          /*store_stencil=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
      },
      // although we have multiple swapchain images, we will share one depth
      // stencil image, because we only use one graphics queue, which only
      // renders on one swapchain image at a time
      /*get_image=*/[&depth_stencil_image](int index) -> const Image& {
        return depth_stencil_image;
      },
      /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
      /*final_layout=*/VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  });

  builder->attachment_references_.push_back({  // color
      /*attachment=*/0,  // index of attachment to reference to
      /*layout=*/VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  });
  builder->attachment_references_.push_back({  // depth stencil
      /*attachment=*/1,
      /*layout=*/VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  });

  builder->subpass_descriptions_.push_back({
      /*flags=*/nullflag,
      /*pipelineBindPoint=*/VK_PIPELINE_BIND_POINT_GRAPHICS,
      /*inputAttachmentCount=*/0,
      /*pInputAttachments=*/nullptr,
      // layout (location = 0) will be rendered to the first attachment
      /*colorAttachmentCount=*/1,
      &builder->attachment_references_[0],
      /*pResolveAttachments=*/nullptr,
      // a render pass can only use one depth stencil buffer, so no count needed
      &builder->attachment_references_[1],
      /*preserveAttachmentCount=*/0,
      /*pPreserveAttachments=*/nullptr,
  });

  // render pass takes care of layout transition, so it has to wait until
  // image is ready. VK_SUBPASS_EXTERNAL means subpass before (if .srcSubpass)
  // or after (if .dstSubpass) render pass
  builder->subpass_dependencies_.push_back({
      /*srcSubpass=*/VK_SUBPASS_EXTERNAL,
      /*dstSubpass=*/0,  // refer to our subpass
      /*srcStageMask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      /*dstStageMask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      /*srcAccessMask=*/0,
      /*dstAccessMask=*/VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                            | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      /*dependencyFlags=*/nullflag,
  });

  return builder;
}

RenderPassBuilder& RenderPassBuilder::set_framebuffer_size(
    VkExtent2D framebuffer_size) {
  framebuffer_size_ = framebuffer_size;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::set_num_framebuffer(int num_framebuffer) {
  num_framebuffer_ = num_framebuffer;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::set_attachment(
    int index, const Attachment& attachment) {
  if (index >= attachments_.size()) {
    attachments_.resize(index + 1);
  }
  attachments_[index] = attachment;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::update_attachment(
    int index, const Attachment::GetImage& get_image) {
  attachments_[index].get_image = get_image;
  return *this;
}

std::unique_ptr<RenderPass> RenderPassBuilder::Build() {
  ASSERT_HAS_VALUE(framebuffer_size_, "Frame size not set");
  ASSERT_HAS_VALUE(num_framebuffer_, "Number of frame not set");

  auto attachment_descriptions = CreateAttachmentDescriptions(attachments_);

  VkRenderPassCreateInfo render_pass_info{
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      CONTAINER_SIZE(attachment_descriptions),
      attachment_descriptions.data(),
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
      context_, render_pass, framebuffer_size_.value(),
      CreateClearValues(attachments_),
      CreateFramebuffers(context_, render_pass, attachments_,
                         framebuffer_size_.value(), num_framebuffer_.value()));
}

void RenderPass::Run(const VkCommandBuffer& command_buffer,
                     int framebuffer_index,
                     const RenderOps& render_ops) const {
  VkRenderPassBeginInfo begin_info{
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      /*pNext=*/nullptr,
      render_pass_,
      framebuffers_[framebuffer_index],
      /*renderArea=*/{
          /*offset=*/{0, 0},
          framebuffer_size_,
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
