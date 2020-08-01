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
          /*load_store_ops=*/Attachment::ColorLoadStoreOps{
              /*color_load_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
              /*color_store_op=*/VK_ATTACHMENT_STORE_OP_STORE,
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
            /*load_store_ops=*/Attachment::DepthStencilLoadStoreOps{
                /*depth_load_op=*/load_op,
                /*depth_store_op=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
                /*stencil_load_op=*/load_op,
                /*stencil_store_op=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
            },
            initial_layout,
            /*final_layout=*/VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        }
    );
  }
  if (use_multisampling) {
    SetAttachment(
        multisample_attachment_index(), Attachment{
            /*load_store_ops=*/Attachment::ColorLoadStoreOps{
                /*color_load_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                /*color_store_op=*/VK_ATTACHMENT_STORE_OP_STORE,
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
  const std::vector<VkAttachmentReference> multisampling_ref =
      RenderPassBuilder::CreateMultisamplingReferences(
          /*num_color_refs=*/1,
          /*infos=*/{
              MultisampleResolveInfo{
                  /*source_location=*/0,
                  /*target_description_index=*/color_attachment_index(),
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              },
          }
      );
  const VkAttachmentReference depth_stencil_ref{
      use_depth_stencil_attachment ? depth_stencil_attachment_index()
                                   : VK_ATTACHMENT_UNUSED,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };
  const auto get_multisampling_refs =
      [use_multisampling, num_subpasses, this](int subpass)
          -> std::vector<VkAttachmentReference> {
        if (!use_multisampling || subpass != num_subpasses - 1) {
          return {};
        }
        return RenderPassBuilder::CreateMultisamplingReferences(
            /*num_color_refs=*/1,
            /*infos=*/{
                MultisampleResolveInfo{
                    /*source_location=*/0,
                    /*target_description_index=*/color_attachment_index(),
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                },
           }
        );
      };

  int subpass_index = 0;
  for (int i = 0; i < num_subpasses_with_depth_stencil_attachment; ++i) {
    SetSubpass(subpass_index,
               std::vector<VkAttachmentReference>{color_refs},
               get_multisampling_refs(subpass_index),
               depth_stencil_ref);
    ++subpass_index;
  }
  for (int i = 0; i < subpass_config.num_overlay_subpasses; ++i) {
    SetSubpass(subpass_index,
               std::vector<VkAttachmentReference>{color_refs},
               get_multisampling_refs(subpass_index),
               /*depth_stencil_ref=*/absl::nullopt);
    ++subpass_index;
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
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
