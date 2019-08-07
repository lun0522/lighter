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
#include "jessie_steamer/wrapper/vulkan/util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

namespace util = common::util;

using std::vector;

using ColorOps = RenderPassBuilder::Attachment::ColorOps;
using DepthStencilOps = RenderPassBuilder::Attachment::DepthStencilOps;

VkClearValue CreateClearColor(const RenderPassBuilder::Attachment& attachment) {
  VkClearValue clear_value{};
  if (absl::holds_alternative<ColorOps>(attachment.attachment_ops)) {
    clear_value.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  } else if (absl::holds_alternative<DepthStencilOps>(
      attachment.attachment_ops)) {
    clear_value.depthStencil = {/*depth=*/1.0f, /*stencil=*/0};
  } else {
    FATAL("Unrecognized variant type");
  }
  return clear_value;
}

VkAttachmentDescription CreateAttachmentDescription(
    const RenderPassBuilder::Attachment& attachment,
    VkFormat format, VkSampleCountFlagBits sample_count) {
  VkAttachmentDescription description{
      /*flags=*/nullflag,
      format,
      sample_count,
      /*loadOp=*/VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // to be updated
      /*storeOp=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,  // to be updated
      /*stencilLoadOp=*/VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // to be updated
      /*stencilStoreOp=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,  // to be updated
      attachment.initial_layout,
      attachment.final_layout,
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
      // https://www.saschawillems.de/blog/2018/07/19/vulkan-input-attachments-and-sub-passes
      /*inputAttachmentCount=*/0,
      /*pInputAttachments=*/nullptr,
      CONTAINER_SIZE(attachments.color_refs),
      attachments.color_refs.data(),
      attachments.multisampling_refs.has_value()
          ? attachments.multisampling_refs.value().data()
          : nullptr,
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

namespace simple_render_pass {

enum AttachmentIndex {
  kColorAttachmentIndex = 0,
  kDepStencilAttachmentIndex,
  kMultisampleAttachmentIndex,
};

} /* namespace simple_render_pass */

vector<VkAttachmentReference> RenderPassBuilder::CreateMultisamplingReferences(
    int num_color_references, const vector<MultisamplingPair>& pairs) {
  if (pairs.empty()) {
    FATAL("No multisampling pairs provided");
  }
  vector<VkAttachmentReference> references(
      num_color_references,
      VkAttachmentReference{
          /*attachment=*/VK_ATTACHMENT_UNUSED,
          /*layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
      }
  );
  for (const auto& pair : pairs) {
    references[pair.multisample_reference] =
        VkAttachmentReference{
            /*attachment=*/static_cast<uint32_t>(pair.target_attachment),
            /*layout=*/VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
  }
  return references;
}

std::unique_ptr<RenderPassBuilder> RenderPassBuilder::SimpleRenderPassBuilder(
    SharedBasicContext context,
    int num_subpass,
    GetImage&& get_depth_stencil_image,
    int num_swapchain_image,
    GetImage&& get_swapchain_image,
    GetImage&& get_multisample_image) {
  auto builder = absl::make_unique<RenderPassBuilder>(std::move(context));

  (*builder)
      .set_num_framebuffer(num_swapchain_image)
      .set_attachment(
          simple_render_pass::kColorAttachmentIndex,
          Attachment{
              /*attachment_ops=*/Attachment::ColorOps{
                  /*load_color=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                  /*store_color=*/VK_ATTACHMENT_STORE_OP_STORE,
              },
              /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
              /*final_layout=*/VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
          },
          std::move(get_swapchain_image)
      )
      .set_attachment(
          simple_render_pass::kDepStencilAttachmentIndex,
          Attachment{
              /*attachment_ops=*/Attachment::DepthStencilOps{
                  /*load_depth=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                  /*store_depth=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
                  /*load_stencil=*/VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                  /*store_stencil=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
              },
              // we don't care about the content previously stored in the depth
              // stencil buffer, so even if it has been transitioned to the
              // optimal layout, we still use undefined as initial layout.
              /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
              /*final_layout=*/VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          },
          // although we have multiple swapchain images, we will share one depth
          // stencil image, because we only use one graphics queue, which only
          // renders on one swapchain image at a time
          std::move(get_depth_stencil_image)
      )
      .set_attachment(
          simple_render_pass::kMultisampleAttachmentIndex,
          Attachment{
              /*attachment_ops=*/Attachment::ColorOps{
                  /*load_color=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                  /*store_color=*/VK_ATTACHMENT_STORE_OP_STORE,
              },
              /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
              /*final_layout=*/VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          },
          std::move(get_multisample_image)
      )
      .set_subpass_description(/*index=*/0, SubpassAttachments{
          /*color_refs=*/{
              VkAttachmentReference{
                  simple_render_pass::kMultisampleAttachmentIndex,
                  /*layout=*/VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              },
          },
          /*multisampling_refs=*/CreateMultisamplingReferences(
              /*num_color_references=*/1,
              /*pairs=*/vector<MultisamplingPair>{
                  MultisamplingPair{
                      /*multisample_reference=*/0,
                      simple_render_pass::kColorAttachmentIndex,
                  },
              }
          ),
          /*depth_stencil_ref=*/VkAttachmentReference{
              /*attachment=*/1,
              /*layout=*/VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          },
      })
      .add_subpass_dependency(SubpassDependency{
          /*src_info=*/SubpassDependency::SubpassInfo{
              /*index=*/VK_SUBPASS_EXTERNAL,
              /*stage_mask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
              /*access_mask=*/VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          },
          /*dst_info=*/SubpassDependency::SubpassInfo{
              /*index=*/0,
              /*stage_mask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
              /*access_mask=*/VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                                  | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          },
      });

  for (uint32_t subpass = 1; subpass < num_subpass; ++subpass) {
    (*builder)
        .set_subpass_description(/*index=*/subpass, SubpassAttachments{
            /*color_refs=*/{
                VkAttachmentReference{
                    simple_render_pass::kColorAttachmentIndex,
                    /*layout=*/VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                },
            },
            /*multisampling_refs=*/absl::nullopt,
            /*depth_stencil_ref=*/absl::nullopt,
        })
        .add_subpass_dependency(SubpassDependency{
            /*src_info=*/SubpassDependency::SubpassInfo{
                /*index=*/subpass - 1,
                /*stage_mask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                /*access_mask=*/VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            },
            /*dst_info=*/SubpassDependency::SubpassInfo{
                /*index=*/subpass,
                /*stage_mask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                /*access_mask=*/VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                                    | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            },
        });
  }

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
  ASSERT_NON_NULL(get_image, "get_image cannot be nullptr");
  const Image& sample_image = get_image(0);
  VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
  if (const auto* multisample_image =
      dynamic_cast<const MultisampleImage*>(&sample_image)) {
    sample_count = multisample_image->sample_count();
  }
  util::SetElementWithResizing(std::move(get_image), index, &get_images_);
  util::SetElementWithResizing(CreateClearColor(attachment),
                               index, &clear_values_);
  util::SetElementWithResizing(
      CreateAttachmentDescription(
          attachment, sample_image.format(), sample_count),
      index, &attachment_descriptions_);
  return *this;
}

RenderPassBuilder& RenderPassBuilder::set_subpass_description(
    int index, SubpassAttachments&& attachments) {
  // note that we need to take address of each SubpassAttachments, hence we need
  // to use a list to ensure they won't be moved
  subpass_attachments_.emplace_back(std::move(attachments));
  util::SetElementWithResizing(
      CreateSubpassDescription(subpass_attachments_.back()),
      index, &subpass_descriptions_);
  return *this;
}

RenderPassBuilder& RenderPassBuilder::add_subpass_dependency(
    const SubpassDependency& dependency) {
  subpass_dependencies_.emplace_back(CreateSubpassDependency(dependency));
  return *this;
}

std::unique_ptr<RenderPass> RenderPassBuilder::Build() const {
  ASSERT_HAS_VALUE(framebuffer_size_, "Frame size is not set");
  ASSERT_HAS_VALUE(num_framebuffer_, "Number of frame is not set");

  for (int i = 0; i < get_images_.size(); ++i) {
    ASSERT_NON_NULL(get_images_[i],
                    absl::StrFormat("Attachment at index %d is not set", i));
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
      context_, /*num_subpass=*/subpass_descriptions_.size(), render_pass,
      framebuffer_size_.value(), clear_values_,
      CreateFramebuffers(context_, render_pass, get_images_,
                         framebuffer_size_.value(), num_framebuffer_.value()));
}

void RenderPass::Run(const VkCommandBuffer& command_buffer,
                     int framebuffer_index,
                     const vector<RenderOp>& render_ops) const {
  if (render_ops.size() != num_subpass_) {
    FATAL(absl::StrFormat("Render pass contains %d subpasses, but %d render "
                          "operations are provided",
                          num_subpass_, render_ops.size()));
  }

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
  for (int i = 0; i < render_ops.size(); ++i) {
    if (i != 0) {
      vkCmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);
    }
    render_ops[i](command_buffer);
  }
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
