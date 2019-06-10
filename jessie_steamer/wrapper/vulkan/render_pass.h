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
 */
class RenderPass {
 public:
  using RenderOps = std::function<void()>;

  RenderPass(SharedBasicContext context,
             const VkRenderPass& render_pass,
             std::vector<VkClearValue>&& clear_values,
             std::vector<VkFramebuffer>&& framebuffers)
    : context_{std::move(context)}, render_pass_{render_pass},
      clear_values_{std::move(clear_values)},
      framebuffers_{std::move(framebuffers)} {}

  // This class is neither copyable nor movable.
  RenderPass(const RenderPass&) = delete;
  RenderPass& operator=(const RenderPass&) = delete;

  ~RenderPass();

  void Run(const VkCommandBuffer& command_buffer,
           int framebuffer_index,
           VkExtent2D frame_size,
           const RenderOps& render_ops) const;

  const VkRenderPass& operator*() const { return render_pass_; }

 private:
  SharedBasicContext context_;
  VkRenderPass render_pass_;
  std::vector<VkClearValue> clear_values_;
  std::vector<VkFramebuffer> framebuffers_;
};

class RenderPassBuilder {
 public:
  static RenderPassBuilder DefaultBuilder(
      SharedBasicContext context,
      const Swapchain& swapchain,
      const DepthStencilImage& depth_stencil_image);

  explicit RenderPassBuilder(SharedBasicContext context)
    : context_{std::move(context)} {}

  // This class is only movable.
  RenderPassBuilder(RenderPassBuilder&&) = default;
  RenderPassBuilder& operator=(RenderPassBuilder&&) = default;

  // Build() can be called multiple times.
  std::unique_ptr<RenderPass> Build(
      const Swapchain& swapchain,
      const DepthStencilImage& depth_stencil_image);

 private:
  SharedBasicContext context_;
  std::vector<VkAttachmentDescription> attachment_descriptions_;
  std::vector<VkAttachmentReference> attachment_references_;
  std::vector<VkSubpassDescription> subpass_descriptions_;
  std::vector<VkSubpassDependency> subpass_dependencies_;
  std::vector<VkClearValue> clear_values_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_H */
