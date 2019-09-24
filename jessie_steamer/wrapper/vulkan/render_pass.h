//
//  render_pass.h
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_H

#include <functional>
#include <list>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace simple_render_pass {

enum AttachmentIndex {
  kColorAttachmentIndex = 0,
  kDepthStencilAttachmentIndex,
  kMultisampleAttachmentIndex,
};

} /* namespace simple_render_pass */

/** VkRenderPass specifies types of attachments that will be accessed.
 *
 *  Initialization:
 *    VkDevice
 *    List of VkAttachmentDescription
 *    List of VkSubpassDescription
 *    List of VkSubpassDependency
 *
 *------------------------------------------------------------------------------
 *
 *  VkFramebuffer specifies actual image views to bind to attachments.
 *
 *  Initialization:
 *    VkRenderPass
 *    List of VkImageView
 *    Image extent (width, height and number of layers)
 *
 *------------------------------------------------------------------------------
 *
 *  VkAttachmentDescription describes how do we use attachments.
 *
 *  VkAttachmentLoadOp: LOAD / CLEAR / DONT_CARE
 *  VkAttachmentStoreOp: STORE / DONT_STORE
 *
 *  VkImageLayout specifies layout of pixels in memory. Commonly used options:
 *    - VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: for color attachment
 *    - VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: for images in swapchain
 *    - VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: for images as destination
 *                                            for memory copy
 *    - VK_IMAGE_LAYOUT_UNDEFINED: don't care about layout before this render
 *                                 pass
 */
class RenderPass {
 public:
  using RenderOp = std::function<void(const VkCommandBuffer&)>;

  RenderPass(SharedBasicContext context,
             int num_subpass,
             const VkRenderPass& render_pass,
             VkExtent2D framebuffer_size,
             std::vector<VkClearValue> clear_values,
             std::vector<VkFramebuffer>&& framebuffers)
    : context_{std::move(context)},
      num_subpass_{num_subpass},
      render_pass_{render_pass},
      framebuffer_size_{framebuffer_size},
      clear_values_{std::move(clear_values)},
      framebuffers_{std::move(framebuffers)} {}

  // This class is neither copyable nor movable.
  RenderPass(const RenderPass&) = delete;
  RenderPass& operator=(const RenderPass&) = delete;

  ~RenderPass();

  void Run(const VkCommandBuffer& command_buffer,
           int framebuffer_index,
           const std::vector<RenderOp>& render_ops) const;

  const VkRenderPass& operator*() const { return render_pass_; }

 private:
  const SharedBasicContext context_;
  const int num_subpass_;
  VkRenderPass render_pass_;
  VkExtent2D framebuffer_size_;
  std::vector<VkClearValue> clear_values_;
  std::vector<VkFramebuffer> framebuffers_;
};

class RenderPassBuilder {
 public:
  using GetImage = std::function<const Image&(int index)>;

  struct Attachment {
    struct ColorOps {
      VkAttachmentLoadOp load_color;
      VkAttachmentStoreOp store_color;
    };

    struct DepthStencilOps {
      VkAttachmentLoadOp load_depth;
      VkAttachmentStoreOp store_depth;
      VkAttachmentLoadOp load_stencil;
      VkAttachmentStoreOp store_stencil;
    };

    absl::variant<ColorOps, DepthStencilOps> attachment_ops;
    VkImageLayout initial_layout;
    VkImageLayout final_layout;
  };

  struct SubpassAttachments {
    std::vector<VkAttachmentReference> color_refs;
    absl::optional<std::vector<VkAttachmentReference>> multisampling_refs;
    absl::optional<VkAttachmentReference> depth_stencil_ref;
  };

  struct SubpassDependency {
    struct SubpassInfo {
      // We may use VK_SUBPASS_EXTERNAL to refer to the subpass before
      // (if src.index) or after (if dst.index) another subpass.
      uint32_t index;

      // Frequently used options:
      //  - VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT: if we want to
      //      read/write to the color attachment.
      //  - VK_PIPELINE_STAGE_EARLY/LATE_FRAGMENT_TESTS_BIT: if we want to
      //      read/write to the depth stencil buffer.
      //  - VK_PIPELINE_STAGE_VERTEX/FRAGMENT_SHADER_BIT: if we only want to
      //      read (sample) the attachment.
      // This should always be non-zero.
      VkPipelineStageFlags stage_mask;

      // Frequently used options:
      //  - VK_ACCESS_SHADER_READ/WRITE_BIT: if we want to sample a texture or
      //      read/write to a buffer.
      //  - VK_ACCESS_COLOR/DEPTH_STENCIL_ATTACHMENT_READ/WRITE_BIT: if we want
      //      to read/write to an attachment.
      //  - VK_ACCESS_INPUT_ATTACHMENT_READ_BIT: if we use inputAttachment, in
      //      which case we also need to specify dependencyFlags.
      // If the previous subpass does not write to the attachment (in which case
      // the attachment should be in the READ_ONLY layout), and we need to write
      // to it (should be in the ATTACHMENT layout), we can put a 0 here, and
      // the transition of layouts will insert a memory barrier.
      VkAccessFlags access_mask;
    };
    SubpassInfo src_info, dst_info;
  };

  // Specifies which attachment need to be resolved to the target attachment.
  // Note that 'multisample_reference' is the index of VkAttachmentReference,
  // while 'target_attachment' is the index of VkAttachmentDescription.
  struct MultisamplingPair {
    int multisample_reference;
    int target_attachment;
  };

  static std::vector<VkAttachmentReference> CreateMultisamplingReferences(
      int num_color_references, const std::vector<MultisamplingPair>& pairs);

  // Contains one color attachment (at index 0) and one depth attachment (at
  // index 1). Only the first subpass will use the depth attachment and is
  // intended for rendering opaque objects. Following subpasses are intended for
  // transparent objects and text. Each of them will depend on the previous one.
  // Only set_framebuffer_size() and update_image() need to be called when the
  // window is resized.
  static std::unique_ptr<RenderPassBuilder> SimpleRenderPassBuilder(
      SharedBasicContext context,
      int num_subpass, int num_swapchain_image,
      absl::optional<MultisampleImage::Mode> multisampling_mode);

  explicit RenderPassBuilder(SharedBasicContext context)
    : context_{std::move(context)} {}

  // This class is neither copyable nor movable.
  RenderPassBuilder(const RenderPassBuilder&) = delete;
  RenderPassBuilder& operator=(const RenderPassBuilder&) = delete;

  // All these information must be set before Build().
  RenderPassBuilder& set_framebuffer_size(VkExtent2D size);
  RenderPassBuilder& set_num_framebuffer(int count);
  RenderPassBuilder& set_attachment(int index, const Attachment& attachment);
  RenderPassBuilder& update_image(int index, GetImage&& get_image);
  RenderPassBuilder& set_subpass_description(int index,
                                             SubpassAttachments&& attachments);
  RenderPassBuilder& add_subpass_dependency(
      const SubpassDependency& dependency);

  // Build() can be called multiple times.
  std::unique_ptr<RenderPass> Build() const;

 private:
  const SharedBasicContext context_;
  absl::optional<VkExtent2D> framebuffer_size_;
  absl::optional<int> num_framebuffer_;
  std::vector<GetImage> get_images_;
  std::vector<VkClearValue> clear_values_;
  std::list<SubpassAttachments> subpass_attachments_;
  std::vector<VkAttachmentDescription> attachment_descriptions_;
  std::vector<VkSubpassDescription> subpass_descriptions_;
  std::vector<VkSubpassDependency> subpass_dependencies_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_H */
