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
      .set_num_framebuffer(num_swapchain_image)
      .set_attachment(
          kColorAttachmentIndex,
          Attachment{
              /*attachment_ops=*/Attachment::ColorOps{
                  /*load_color=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                  /*store_color=*/VK_ATTACHMENT_STORE_OP_STORE,
              },
              /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
              /*final_layout=*/VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
          }
      )
      .set_attachment(
          kDepthStencilAttachmentIndex,
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
          }
      )
      .add_subpass_dependency(SubpassDependency{
          /*src_info=*/SubpassDependency::SubpassInfo{
              /*index=*/VK_SUBPASS_EXTERNAL,
              /*stage_mask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
              /*access_mask=*/VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          },
          /*dst_info=*/SubpassDependency::SubpassInfo{
              /*index=*/0,
              /*stage_mask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
              /*access_mask=*/
              VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                  | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          },
      });

  if (multisampling_mode.has_value()) {
    (*builder)
        .set_attachment(
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
        .set_subpass_description(/*index=*/0, SubpassAttachments{
            /*color_refs=*/{
                VkAttachmentReference{
                    kMultisampleAttachmentIndex,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                },
            },
            /*multisampling_refs=*/
            RenderPassBuilder::CreateMultisamplingReferences(
                /*num_color_references=*/1,
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
    builder->set_subpass_description(
        /*index=*/0, SubpassAttachments{
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

  for (uint32_t subpass = 1; subpass < num_subpass; ++subpass) {
    (*builder)
        .set_subpass_description(/*index=*/subpass, SubpassAttachments{
            /*color_refs=*/{
                VkAttachmentReference{
                    kColorAttachmentIndex,
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
