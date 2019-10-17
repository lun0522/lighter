//
//  render_pass_util.cc
//
//  Created by Pujun Lun on 9/24/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/render_pass_util.h"

#include <vector>

#include "third_party/absl/memory/memory.h"
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

namespace naive_render_pass {

std::unique_ptr<RenderPassBuilder> GetRenderPassBuilder(
    SharedBasicContext context, const SubpassConfig& subpass_config,
    int num_framebuffers, bool present_to_screen,
    absl::optional<MultisampleImage::Mode> multisampling_mode) {
  auto builder = absl::make_unique<RenderPassBuilder>(std::move(context));

  /* Framebuffers and attachments */
  const int num_subpasses_with_depth_attachment =
      (subpass_config.use_opaque_subpass ? 1 : 0) +
      subpass_config.num_transparent_subpasses;
  const bool use_multisampling = (num_subpasses_with_depth_attachment > 0) &&
                                 multisampling_mode.has_value();
  (*builder)
      .SetNumFramebuffers(num_framebuffers)
      .SetAttachment(
          kColorAttachmentIndex, Attachment{
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
  if (num_subpasses_with_depth_attachment > 0) {
    builder->SetAttachment(
        kDepthAttachmentIndex, Attachment{
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
  if (use_multisampling) {
    builder->SetAttachment(
        kMultisampleAttachmentIndex, Attachment{
            /*attachment_ops=*/Attachment::ColorOps{
                /*load_color_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                /*store_color_op=*/VK_ATTACHMENT_STORE_OP_STORE,
            },
            /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
            /*final_layout=*/VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        }
    );
  }

  /* Subpasses */
  vector<VkAttachmentReference> color_refs{
      VkAttachmentReference{
          use_multisampling ? kMultisampleAttachmentIndex
                            : kColorAttachmentIndex,
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      },
  };
  const VkAttachmentReference depth_stencil_ref{
      kDepthAttachmentIndex,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };
  int subpass_index = 0;
  for (int i = 0; i < num_subpasses_with_depth_attachment; ++i) {
    builder->SetSubpass(
        subpass_index++,
        vector<VkAttachmentReference>{color_refs},
        depth_stencil_ref
    );
  }
  color_refs[0].attachment = kColorAttachmentIndex;
  for (int i = 0; i < subpass_config.num_post_processing_subpasses; ++i) {
    builder->SetSubpass(
        subpass_index++,
        vector<VkAttachmentReference>{color_refs},
        /*depth_stencil_ref=*/absl::nullopt
    );
  }
  const int num_subpasses = subpass_index;

  /* Subpass dependencies */
  for (uint32_t index = 0; index < num_subpasses; ++index) {
    const uint32_t prev_subpass_index =
        (index > 0) ? (index - 1) : kExternalSubpassIndex;
    (*builder)
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
  if (use_multisampling) {
    builder->SetMultisampling(
        /*subpass_index=*/num_subpasses_with_depth_attachment - 1,
        RenderPassBuilder::CreateMultisamplingReferences(
            /*num_color_refs=*/1,
            /*pairs=*/vector<MultisamplingPair>{
                MultisamplingPair{
                    /*multisample_reference=*/0,
                    /*target_attachment=*/kColorAttachmentIndex,
                },
            }
        )
    );
  }

  return builder;
}

} /* namespace naive_render_pass */

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
