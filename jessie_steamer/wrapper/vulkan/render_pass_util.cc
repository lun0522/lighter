//
//  render_pass_util.cc
//
//  Created by Pujun Lun on 9/24/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/render_pass_util.h"

#include <vector>

#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::vector;
using Attachment = RenderPassBuilder::Attachment;
using MultisamplingPair = RenderPassBuilder::MultisamplingPair;
using SubpassAttachments = RenderPassBuilder::SubpassAttachments;
using SubpassDependency = RenderPassBuilder::SubpassDependency;
using SubpassInfo = SubpassDependency::SubpassInfo;

} /* namespace */

NaiveRenderPassBuilder::NaiveRenderPassBuilder(
    SharedBasicContext context, const SubpassConfig& subpass_config,
    int num_framebuffers, bool present_to_screen,
    absl::optional<MultisampleImage::Mode> multisampling_mode)
    : builder_{std::move(context)} {
  const int num_subpasses_with_depth_attachment =
      (subpass_config.use_opaque_subpass ? 1 : 0) +
      subpass_config.num_transparent_subpasses;
  const int num_subpasses = num_subpasses_with_depth_attachment +
                            subpass_config.num_overlay_subpasses;

  const bool use_depth_attachment = (num_subpasses_with_depth_attachment > 0);
  if (use_depth_attachment) {
    depth_attachment_index_ = color_attachment_index_ + 1;
  }

  const bool use_multisample_attachment = multisampling_mode.has_value();
  if (use_multisample_attachment) {
    multisample_attachment_index_ =
        1 + (use_depth_attachment ? depth_attachment_index_.value()
                                  : color_attachment_index_);
  }

  /* Framebuffers and attachments */
  builder_
      .SetNumFramebuffers(num_framebuffers)
      .SetAttachment(
          color_attachment_index(), Attachment{
              /*attachment_ops=*/Attachment::ColorOps{
                  /*load_color_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                  /*store_color_op=*/VK_ATTACHMENT_STORE_OP_STORE,
              },
              /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
              /*final_layout=*/
              present_to_screen ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          }
      );
  if (use_depth_attachment) {
    builder_.SetAttachment(
        depth_attachment_index(), Attachment{
            /*attachment_ops=*/Attachment::DepthStencilOps{
                /*load_depth_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                /*store_depth_op=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
                /*load_stencil_op=*/VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                /*store_stencil_op=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
            },
            // We don't care about the content previously stored in the depth
            // stencil buffer, so even if it has been transitioned to the
            // optimal layout, we still use UNDEFINED as the initial layout.
            /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
            /*final_layout=*/VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        }
    );
  }
  if (use_multisample_attachment) {
    builder_.SetAttachment(
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
  const vector<VkAttachmentReference> color_refs{
      VkAttachmentReference{
          use_multisample_attachment ? multisample_attachment_index()
                                     : color_attachment_index(),
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      },
  };
  const VkAttachmentReference depth_stencil_ref{
      use_depth_attachment ? depth_attachment_index() : VK_ATTACHMENT_UNUSED,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };
  int subpass_index = 0;
  for (int i = 0; i < num_subpasses_with_depth_attachment; ++i) {
    builder_.SetSubpass(
        subpass_index++,
        vector<VkAttachmentReference>{color_refs},
        depth_stencil_ref
    );
  }
  for (int i = 0; i < subpass_config.num_overlay_subpasses; ++i) {
    builder_.SetSubpass(
        subpass_index++,
        vector<VkAttachmentReference>{color_refs},
        /*depth_stencil_ref=*/absl::nullopt
    );
  }

  /* Subpass dependencies */
  for (uint32_t index = 0; index < num_subpasses; ++index) {
    const uint32_t prev_subpass_index =
        (index > 0) ? (index - 1) : kExternalSubpassIndex;
    builder_
        .AddSubpassDependency(SubpassDependency{
            /*prev_subpass=*/SubpassInfo{
                prev_subpass_index,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            },
            /*next_subpass=*/SubpassInfo{
                index,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                    | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            },
        });
  }

  /* Multisampling */
  if (use_multisample_attachment) {
    builder_.SetMultisampling(
        /*subpass_index=*/num_subpasses - 1,
        RenderPassBuilder::CreateMultisamplingReferences(
            /*num_color_refs=*/1,
            /*pairs=*/vector<MultisamplingPair>{
                MultisamplingPair{
                    /*multisample_reference=*/0,
                    /*target_attachment=*/color_attachment_index(),
                },
            }
        )
    );
  }
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
