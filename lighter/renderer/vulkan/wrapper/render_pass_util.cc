//
//  render_pass_util.cc
//
//  Created by Pujun Lun on 9/24/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/render_pass_util.h"

#include <vector>

#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

using Attachment = RenderPassBuilder::Attachment;
using MultisamplingPair = RenderPassBuilder::MultisamplingPair;
using SubpassAttachments = RenderPassBuilder::SubpassAttachments;
using SubpassDependency = RenderPassBuilder::SubpassDependency;
using SubpassInfo = SubpassDependency::SubpassInfo;

// Returns the final layout of color attachment based on its usage.
VkImageLayout GetColorAttachmentFinalLayout(
    NaiveRenderPassBuilder::ColorAttachmentFinalUsage usage) {
  using Usage = NaiveRenderPassBuilder::ColorAttachmentFinalUsage;
  switch (usage) {
    case Usage::kPresentToScreen:
      return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case Usage::kSampledAsTexture:
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case Usage::kAccessedLinearly:
      return VK_IMAGE_LAYOUT_GENERAL;
  }
}

} /* namespace */

NaiveRenderPassBuilder::NaiveRenderPassBuilder(
    SharedBasicContext context, const SubpassConfig& subpass_config,
    int num_framebuffers, bool use_multisampling,
    ColorAttachmentFinalUsage color_attachment_final_usage,
    bool preserve_depth_stencil_attachment_content)
    : RenderPassBuilder{std::move(context)} {
  const int num_subpasses_with_depth_stencil_attachment =
      (subpass_config.use_opaque_subpass ? 1 : 0) +
      subpass_config.num_transparent_subpasses;
  const int num_subpasses = num_subpasses_with_depth_stencil_attachment +
                            subpass_config.num_overlay_subpasses;

  const bool use_depth_stencil_attachment =
      num_subpasses_with_depth_stencil_attachment > 0;
  if (use_depth_stencil_attachment) {
    depth_stencil_attachment_index_ = color_attachment_index() + 1;
  }

  if (use_multisampling) {
    multisample_attachment_index_ =
        1 + (use_depth_stencil_attachment
                 ? depth_stencil_attachment_index_.value()
                 : color_attachment_index());
  }

  /* Framebuffers and attachments */
  SetNumFramebuffers(num_framebuffers);
  SetAttachment(
      color_attachment_index(), Attachment{
          /*attachment_ops=*/Attachment::ColorOps{
              /*load_color_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
              /*store_color_op=*/VK_ATTACHMENT_STORE_OP_STORE,
          },
          /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
          GetColorAttachmentFinalLayout(color_attachment_final_usage),
      }
  );
  if (use_depth_stencil_attachment) {
    VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (preserve_depth_stencil_attachment_content) {
      load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
      initial_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    SetAttachment(
        depth_stencil_attachment_index(), Attachment{
            /*attachment_ops=*/Attachment::DepthStencilOps{
                /*load_depth_op=*/load_op,
                /*store_depth_op=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
                /*load_stencil_op=*/load_op,
                /*store_stencil_op=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
            },
            initial_layout,
            /*final_layout=*/VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        }
    );
  }
  if (use_multisampling) {
    SetAttachment(
        multisample_attachment_index(), Attachment{
            /*attachment_ops=*/Attachment::ColorOps{
                /*load_color_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                /*store_color_op=*/VK_ATTACHMENT_STORE_OP_STORE,
            },
            /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
            /*final_layout=*/VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        }
    );
  }

  /* Subpasses descriptions */
  const std::vector<VkAttachmentReference> color_refs{
      VkAttachmentReference{
          static_cast<uint32_t>(
              use_multisampling ? multisample_attachment_index()
                                : color_attachment_index()),
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      },
  };
  const VkAttachmentReference depth_stencil_ref{
      use_depth_stencil_attachment ? depth_stencil_attachment_index()
                                   : VK_ATTACHMENT_UNUSED,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };
  int subpass_index = 0;
  for (int i = 0; i < num_subpasses_with_depth_stencil_attachment; ++i) {
    SetSubpass(subpass_index++,
               std::vector<VkAttachmentReference>{color_refs},
               depth_stencil_ref);
  }
  for (int i = 0; i < subpass_config.num_overlay_subpasses; ++i) {
    SetSubpass(subpass_index++,
               std::vector<VkAttachmentReference>{color_refs},
               /*depth_stencil_ref=*/absl::nullopt);
  }

  /* Subpass dependencies */
  AddSubpassDependency(SubpassDependency{
      /*src_subpass=*/SubpassInfo{
          kExternalSubpassIndex,
          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
          VK_ACCESS_MEMORY_READ_BIT,
      },
      /*dst_subpass=*/SubpassInfo{
          /*index=*/0,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
              | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      },
      /*dependency_flags=*/nullflag,
  });
  for (uint32_t index = 0; index < num_subpasses; ++index) {
    const uint32_t src_subpass_index =
        (index > 0) ? (index - 1) : kExternalSubpassIndex;
    AddSubpassDependency(SubpassDependency{
        /*src_subpass=*/SubpassInfo{
            src_subpass_index,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        },
        /*dst_subpass=*/SubpassInfo{
            index,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        },
        /*dependency_flags=*/nullflag,
    });
  }

  /* Multisampling */
  if (use_multisampling) {
    SetMultisampling(
        /*subpass_index=*/num_subpasses - 1,
        RenderPassBuilder::CreateMultisamplingReferences(
            /*num_color_refs=*/1,
            /*pairs=*/{
                MultisamplingPair{
                    /*multisample_reference=*/0,
                    /*target_attachment=*/
                    static_cast<uint32_t>(color_attachment_index()),
                },
            }
        )
    );
  }
}

DeferredShadingRenderPassBuilder::DeferredShadingRenderPassBuilder(
    SharedBasicContext context, int num_framebuffers, int num_color_attachments)
    : RenderPassBuilder{std::move(context)} {
  /* Framebuffers and attachments */
  SetNumFramebuffers(num_framebuffers);
  SetAttachment(
      depth_stencil_attachment_index(), Attachment{
          /*attachment_ops=*/Attachment::DepthStencilOps{
              /*load_depth_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
              /*store_depth_op=*/VK_ATTACHMENT_STORE_OP_STORE,
              /*load_stencil_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
              /*store_stencil_op=*/VK_ATTACHMENT_STORE_OP_STORE,
          },
          /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
          /*final_layout=*/VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      }
  );
  for (int i = 0; i < num_color_attachments; ++i) {
    SetAttachment(
        color_attachments_index_base() + i, Attachment{
            /*attachment_ops=*/Attachment::ColorOps{
                /*load_color_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                /*store_color_op=*/VK_ATTACHMENT_STORE_OP_STORE,
            },
            /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
            /*final_layout=*/VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }
    );
  }

  /* Subpasses descriptions */
  std::vector<VkAttachmentReference> color_refs;
  color_refs.reserve(num_color_attachments);
  for (int i = 0; i < num_color_attachments; ++i) {
    color_refs.push_back(VkAttachmentReference{
        static_cast<uint32_t>(color_attachments_index_base() + i),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    });
  }
  const VkAttachmentReference depth_stencil_ref{
      static_cast<uint32_t>(depth_stencil_attachment_index()),
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };
  SetSubpass(/*index=*/0, std::move(color_refs), depth_stencil_ref);

  /* Subpass dependencies */
  AddSubpassDependency(SubpassDependency{
      /*src_subpass=*/SubpassInfo{
          kExternalSubpassIndex,
          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
          VK_ACCESS_MEMORY_READ_BIT,
      },
      /*dst_subpass=*/SubpassInfo{
          /*index=*/0,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
              | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      },
      VK_DEPENDENCY_BY_REGION_BIT,
  });
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
