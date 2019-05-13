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

#include "jessie_steamer/wrapper/vulkan/image.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class Context;

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
  RenderPass() = default;

  // This class is neither copyable nor movable
  RenderPass(const RenderPass&) = delete;
  RenderPass& operator=(const RenderPass&) = delete;

  ~RenderPass() { Cleanup(); }

  void Init(const std::shared_ptr<Context>& context);
  void Cleanup();

  const VkRenderPass& operator*() const { return render_pass_; }
  const VkFramebuffer& framebuffer(int index) const
      { return framebuffers_[index]; }

 private:
  std::shared_ptr<Context> context_;
  VkRenderPass render_pass_;
  DepthStencilImage depth_stencil_;
  std::vector<VkFramebuffer> framebuffers_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_H */
