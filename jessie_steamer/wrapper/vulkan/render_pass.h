//
//  render_pass.h
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_H

#include <functional>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/swapchain.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

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
  using RenderOps = std::function<void()>;

  RenderPass(SharedBasicContext context,
             const VkRenderPass& render_pass,
             VkExtent2D framebuffer_size,
             std::vector<VkClearValue>&& clear_values,
             std::vector<VkFramebuffer>&& framebuffers)
    : context_{std::move(context)}, render_pass_{render_pass},
      framebuffer_size_{framebuffer_size},
      clear_values_{std::move(clear_values)},
      framebuffers_{std::move(framebuffers)} {}

  // This class is neither copyable nor movable.
  RenderPass(const RenderPass&) = delete;
  RenderPass& operator=(const RenderPass&) = delete;

  ~RenderPass();

  void Run(const VkCommandBuffer& command_buffer,
           int framebuffer_index,
           const RenderOps& render_ops) const;

  const VkRenderPass& operator*() const { return render_pass_; }

 private:
  SharedBasicContext context_;
  VkRenderPass render_pass_;
  VkExtent2D framebuffer_size_;
  std::vector<VkClearValue> clear_values_;
  std::vector<VkFramebuffer> framebuffers_;
};

class RenderPassBuilder {
 public:
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

    using AttachmentOps = absl::variant<ColorOps, DepthStencilOps>;
    using GetImage = std::function<const Image&(int index)>;

    AttachmentOps attachment_ops;
    GetImage get_image;
    VkImageLayout initial_layout;
    VkImageLayout final_layout;
  };

  // Contains one color attachment (at index 0) and a depth attachment (at
  // index 1). Only set_framebuffer_size() and update_attachment() need to be
  // called when window is resized.
  static std::unique_ptr<RenderPassBuilder> SimpleRenderPassBuilder(
      SharedBasicContext context,
      const Swapchain& swapchain,
      const DepthStencilImage& depth_stencil_image);

  explicit RenderPassBuilder(SharedBasicContext context)
    : context_{std::move(context)} {}

  // This class is neither copyable nor movable.
  RenderPassBuilder(const RenderPassBuilder&) = delete;
  RenderPassBuilder& operator=(const RenderPassBuilder&) = delete;

  // All these information must be set before Build().
  RenderPassBuilder& set_framebuffer_size(VkExtent2D framebuffer_size);
  RenderPassBuilder& set_num_framebuffer(int num_framebuffer);
  RenderPassBuilder& set_attachment(int index, const Attachment& attachment);

  // Used to update the get image method for an attachment. For example, when
  // the window is resized and swapchain images are recreated, this should be
  // called and the render pass should be rebuilt.
  RenderPassBuilder& update_attachment(
      int index, const Attachment::GetImage& get_image);

  // Build() can be called multiple times.
  std::unique_ptr<RenderPass> Build();

 private:
  SharedBasicContext context_;
  std::vector<VkAttachmentReference> attachment_references_;
  std::vector<VkSubpassDescription> subpass_descriptions_;
  std::vector<VkSubpassDependency> subpass_dependencies_;
  absl::optional<VkExtent2D> framebuffer_size_;
  absl::optional<int> num_framebuffer_;
  std::vector<Attachment> attachments_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_H */
