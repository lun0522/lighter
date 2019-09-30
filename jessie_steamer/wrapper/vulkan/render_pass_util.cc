//
//  render_pass_util.cc
//
//  Created by Pujun Lun on 9/24/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/render_pass_util.h"

#include <vector>

#include "absl/memory/memory.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using Attachment = RenderPassBuilder::Attachment;
using MultisamplingPair = RenderPassBuilder::MultisamplingPair;
using SubpassAttachments = RenderPassBuilder::SubpassAttachments;
using SubpassDependency = RenderPassBuilder::SubpassDependency;

} /* namespace */

namespace naive_render_pass {

std::unique_ptr<RenderPassBuilder> GetNaiveRenderPassBuilder(
    SharedBasicContext context,
    int num_subpass, int num_swapchain_image,
    absl::optional<MultisampleImage::Mode> multisampling_mode) {
  auto builder = absl::make_unique<RenderPassBuilder>(std::move(context));

  (*builder)
      .SetNumFramebuffer(num_swapchain_image)
      .SetAttachment(
          kColorAttachmentIndex,
          Attachment{
              /*attachment_ops=*/Attachment::ColorOps{
                  /*load_color_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                  /*store_color_op=*/VK_ATTACHMENT_STORE_OP_STORE,
              },
              /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
              /*final_layout=*/VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
          }
      )
      .SetAttachment(
          kDepthStencilAttachmentIndex,
          Attachment{
              /*attachment_ops=*/Attachment::DepthStencilOps{
                  /*load_depth_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                  /*store_depth_op=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
                  /*load_stencil_op=*/VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                  /*store_stencil_op=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
              },
              // we don't care about the content previously stored in the depth
              // stencil buffer, so even if it has been transitioned to the
              // optimal layout, we still use undefined as initial layout.
              /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
              /*final_layout=*/VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          }
      )
      .AddSubpassDependency(SubpassDependency{
          /*prev_subpass=*/SubpassDependency::SubpassInfo{
              kExternalSubpassIndex,
              /*stage_mask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
              /*access_mask=*/VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          },
          /*next_subpass=*/SubpassDependency::SubpassInfo{
              kNativeSubpassIndex,
              /*stage_mask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
              /*access_mask=*/
              VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                  | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          },
      });

  if (multisampling_mode.has_value()) {
    (*builder)
        .SetAttachment(
            kMultisampleAttachmentIndex,
            Attachment{
                /*attachment_ops=*/Attachment::ColorOps{
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                },
                /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
                /*final_layout=*/VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }
        )
        .SetSubpass(kNativeSubpassIndex, SubpassAttachments{
            /*color_refs=*/{
                VkAttachmentReference{
                    kMultisampleAttachmentIndex,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                },
            },
            /*multisampling_refs=*/
            RenderPassBuilder::CreateMultisamplingReferences(
                /*num_color_refs=*/1,
                /*pairs=*/std::vector<MultisamplingPair>{
                    MultisamplingPair{
                        /*multisample_reference=*/0,
                        /*target_attachment=*/kColorAttachmentIndex,
                    },
                }
            ),
            /*depth_stencil_ref=*/VkAttachmentReference{
                kDepthStencilAttachmentIndex,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            },
        });
  } else {
    builder->SetSubpass(
        kNativeSubpassIndex, SubpassAttachments{
            /*color_refs=*/{
                VkAttachmentReference{
                    kColorAttachmentIndex,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                },
            },
            /*multisampling_refs=*/absl::nullopt,
            /*depth_stencil_ref=*/VkAttachmentReference{
                kDepthStencilAttachmentIndex,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            },
        });
  }

  for (uint32_t subpass = kFirstExtraSubpassIndex;
       subpass < num_subpass; ++subpass) {
    (*builder)
        .SetSubpass(/*index=*/subpass, SubpassAttachments{
            /*color_refs=*/{
                VkAttachmentReference{
                    kColorAttachmentIndex,
                    /*layout=*/VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                },
            },
            /*multisampling_refs=*/absl::nullopt,
            /*depth_stencil_ref=*/absl::nullopt,
        })
        .AddSubpassDependency(SubpassDependency{
            /*prev_subpass=*/SubpassDependency::SubpassInfo{
                /*index=*/subpass - 1,
                /*stage_mask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                /*access_mask=*/VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            },
            /*next_subpass=*/SubpassDependency::SubpassInfo{
                /*index=*/subpass,
                /*stage_mask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                /*access_mask=*/
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                    | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            },
        });
  }

  return builder;
}

} /* namespace naive_render_pass */

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
