//
//  render_pass.h
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_H

#include <memory>
#include <vector>

#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
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
  RenderPass(SharedBasicContext context,
             const VkRenderPass& render_pass,
             std::vector<VkFramebuffer>&& framebuffers)
    : context_{std::move(context)}, render_pass_{render_pass},
      framebuffers_{std::move(framebuffers)} {}

  // This class is neither copyable nor movable.
  RenderPass(const RenderPass&) = delete;
  RenderPass& operator=(const RenderPass&) = delete;

  ~RenderPass();

  const VkRenderPass& operator*() const { return render_pass_; }
  const VkFramebuffer& framebuffer(int index) const
      { return framebuffers_[index]; }

 private:
  SharedBasicContext context_;
  VkRenderPass render_pass_;
  std::vector<VkFramebuffer> framebuffers_;
};

class RenderPassBuilder {
 public:
  explicit RenderPassBuilder(SharedBasicContext context);

  // This class is neither copyable nor movable.
  RenderPassBuilder(const RenderPassBuilder&) = delete;
  RenderPassBuilder& operator=(const RenderPassBuilder&) = delete;

  // All these information must be set before Build().

  // Build() can be called multiple times.
  std::unique_ptr<RenderPass> Build();

 private:
  SharedBasicContext context_;
  std::vector<VkAttachmentDescription> attachment_descriptions_;
  std::vector<VkSubpassDescription> subpass_descriptions_;
  std::vector<VkSubpassDependency> subpass_dependencies_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_H */
