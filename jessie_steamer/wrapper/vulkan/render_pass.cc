//
//  render_pass.cc
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/render_pass.h"

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/macro.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

namespace util = common::util;

using ColorOps = RenderPassBuilder::Attachment::ColorOps;
using DepthStencilOps = RenderPassBuilder::Attachment::DepthStencilOps;
using std::vector;

VkClearValue CreateClearColor(const RenderPassBuilder::Attachment& attachment) {
  VkClearValue clear_value{};
  if (absl::holds_alternative<ColorOps>(attachment.attachment_ops)) {
    auto& color = clear_value.color.float32;
    color[0] = 0.0f;
    color[1] = 0.0f;
    color[2] = 0.0f;
    color[3] = 1.0f;
  } else if (absl::holds_alternative<DepthStencilOps>(
      attachment.attachment_ops)) {
    clear_value.depthStencil = {/*depth=*/1.0f, /*stencil=*/0};
  } else {
    FATAL("Unrecognized variant type");
  }
  return clear_value;
}

VkAttachmentDescription CreateAttachmentDescription(
    const RenderPassBuilder::Attachment& attachment, VkFormat format) {
  VkAttachmentDescription description{
      /*flags=*/nullflag,
      format,
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
    FATAL("Unrecognized variant type");
  }
  return description;
}

VkSubpassDescription CreateSubpassDescription(
    const RenderPassBuilder::SubpassAttachments& attachments) {
  return VkSubpassDescription{
      /*flags=*/nullflag,
      /*pipelineBindPoint=*/VK_PIPELINE_BIND_POINT_GRAPHICS,
      /*inputAttachmentCount=*/0,
      /*pInputAttachments=*/nullptr,
      CONTAINER_SIZE(attachments.color_refs),
      attachments.color_refs.data(),
      /*pResolveAttachments=*/nullptr,
      // render pass can only use one depth stencil buffer, so no count needed
      attachments.depth_stencil_ref.has_value()
          ? &attachments.depth_stencil_ref.value()
          : nullptr,
      /*preserveAttachmentCount=*/0,
      /*pPreserveAttachments=*/nullptr,
  };
}

inline VkSubpassDependency CreateSubpassDependency(
    const RenderPassBuilder::SubpassDependency& dependency) {
  return VkSubpassDependency{
      dependency.src_info.index,
      dependency.dst_info.index,
      dependency.src_info.stage_mask,
      dependency.dst_info.stage_mask,
      dependency.src_info.access_mask,
      dependency.dst_info.access_mask,
      /*dependencyFlags=*/nullflag,
  };
}

vector<VkFramebuffer> CreateFramebuffers(
    const SharedBasicContext& context,
    const VkRenderPass& render_pass,
    const vector<RenderPassBuilder::GetImage>& get_images,
    VkExtent2D framebuffer_size, int num_framebuffer) {
  vector<VkFramebuffer> framebuffers(num_framebuffer);
  for (int i = 0; i < framebuffers.size(); ++i) {
    vector<VkImageView> image_views;
    image_views.reserve(get_images.size());
    for (const auto& get_image : get_images) {
      image_views.emplace_back(get_image(i).image_view());
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
    const DepthStencilImage& depth_stencil_image,
    int num_swapchain_image,
    const GetImage& get_swapchain_image) {
  std::unique_ptr<RenderPassBuilder> builder =
      absl::make_unique<RenderPassBuilder>(std::move(context));

  // number of framebuffer
  builder->set_num_framebuffer(num_swapchain_image);

  // color attachment
  builder->set_attachment(
      /*index=*/0,
      Attachment{
          /*attachment_ops=*/Attachment::ColorOps{
              /*load_color=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
              /*store_color=*/VK_ATTACHMENT_STORE_OP_STORE,
          },
          /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
          /*final_layout=*/VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      },
      GetImage{get_swapchain_image}
  );

  // depth stencil attachment
  builder->set_attachment(
      /*index=*/1,
      Attachment{
          /*attachment_ops=*/Attachment::DepthStencilOps{
              /*load_depth=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
              /*store_depth=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
              /*load_stencil=*/VK_ATTACHMENT_LOAD_OP_DONT_CARE,
              /*store_stencil=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
          },
          /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
          /*final_layout=*/VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      },
      // although we have multiple swapchain images, we will share one depth
      // stencil image, because we only use one graphics queue, which only
      // renders on one swapchain image at a time
      /*get_image=*/[&depth_stencil_image](int index) -> const Image& {
        return depth_stencil_image;
      }
  );

  builder->set_subpass_description(/*index=*/0, SubpassAttachments{
      /*color_refs=*/{
          VkAttachmentReference{
              /*attachment=*/0,
              /*layout=*/VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          },
      },
      /*depth_stencil_ref=*/VkAttachmentReference{
          /*attachment=*/1,
          /*layout=*/VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      },
  });

  builder->add_subpass_dependency(SubpassDependency{
      /*src_info=*/SubpassDependency::SubpassInfo{
          /*index=*/VK_SUBPASS_EXTERNAL,
          /*stage_mask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          /*access_mask=*/0,
      },
      /*dst_info=*/SubpassDependency::SubpassInfo{
          /*index=*/0,
          /*stage_mask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          /*access_mask=*/VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                              | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      },
  });

  return builder;
}

RenderPassBuilder& RenderPassBuilder::set_framebuffer_size(VkExtent2D size) {
  framebuffer_size_ = size;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::set_num_framebuffer(int count) {
  num_framebuffer_ = count;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::set_attachment(
    int index, const Attachment& attachment, GetImage&& get_image) {
  if (get_image == nullptr) {
    FATAL("get_image cannot be nullptr");
  }
  auto format = get_image(0).format();
  util::SetElementWithResizing(&get_images_, index, std::move(get_image));
  util::SetElementWithResizing(&clear_values_, index,
                               CreateClearColor(attachment));
  util::SetElementWithResizing(&attachment_descriptions_, index,
                               CreateAttachmentDescription(attachment, format));
  return *this;
}

RenderPassBuilder& RenderPassBuilder::set_subpass_description(
    int index, SubpassAttachments&& attachments) {
  subpass_attachments_.emplace_back(std::move(attachments));
  util::SetElementWithResizing(
      &subpass_descriptions_, index,
      CreateSubpassDescription(subpass_attachments_.back()));
  return *this;
}

RenderPassBuilder& RenderPassBuilder::add_subpass_dependency(
    const SubpassDependency& dependency) {
  subpass_dependencies_.emplace_back(CreateSubpassDependency(dependency));
  return *this;
}

RenderPassBuilder& RenderPassBuilder::update_attachment(int index,
                                                        GetImage&& get_image) {
  if (index >= get_images_.size()) {
    FATAL(absl::StrFormat("Attachment at index %d is not set", index));
  }
  get_images_[index] = std::move(get_image);
  return *this;
}

std::unique_ptr<RenderPass> RenderPassBuilder::Build() const {
  ASSERT_HAS_VALUE(framebuffer_size_, "Frame size is not set");
  ASSERT_HAS_VALUE(num_framebuffer_, "Number of frame is not set");

  for (int i = 0; i < get_images_.size(); ++i) {
    if (get_images_[i] == nullptr) {
      FATAL(absl::StrFormat("Attachment at index %d is not set", i));
    }
  }

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
      context_, render_pass, framebuffer_size_.value(), clear_values_,
      CreateFramebuffers(context_, render_pass, get_images_,
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
