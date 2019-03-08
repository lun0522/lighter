//
//  render_pass.h
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef VULKAN_WRAPPER_RENDER_PASS_H
#define VULKAN_WRAPPER_RENDER_PASS_H

#include <vector>

#include <vulkan/vulkan.hpp>

namespace vulkan {
namespace wrapper {

class Context;

/** VkRenderPass specifies types of attachments that will be accessed.
 *
 *  Initialization:
 *      VkDevice
 *      List of VkAttachmentDescription
 *      List of VkSubpassDescription
 *      List of VkSubpassDependency
 *
 *------------------------------------------------------------------------------
 *
 *  VkFramebuffer specifies actual image views to bind to attachments.
 *
 *  Initialization:
 *      VkRenderPass
 *      List of VkImageView
 *      Image extent (width, height and number of layers)
 */
class RenderPass {
 public:
  void Init(std::shared_ptr<Context> context);
  void Cleanup();
  ~RenderPass() { Cleanup(); }

  // This class is not copyable or movable
  RenderPass(const RenderPass&) = delete;
  RenderPass& operator=(const RenderPass&) = delete;

  const VkRenderPass& operator*(void) const { return render_pass_; }
  const std::vector<VkFramebuffer>&
      framebuffers() const { return framebuffers_; }

 private:
  std::shared_ptr<Context> context_;
  VkRenderPass render_pass_;
  std::vector<VkFramebuffer> framebuffers_;
};

} /* namespace wrapper */
} /* namespace vulkan */

#endif /* VULKAN_WRAPPER_RENDER_PASS_H */
