//
//  render_pass.h
//  LearnVulkan
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LEARNVULKAN_RENDER_PASS_H
#define LEARNVULKAN_RENDER_PASS_H

#include <vector>

#include <vulkan/vulkan.hpp>

#include "util.h"

namespace vulkan {

class Application;

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
  RenderPass(const Application& app) : app_{app} {}
  void Init();
  void Cleanup();
  ~RenderPass() { Cleanup(); }
  MARK_NOT_COPYABLE_OR_MOVABLE(RenderPass);

  const VkRenderPass& operator*(void) const { return render_pass_; }
  const std::vector<VkFramebuffer>&
      framebuffers() const { return framebuffers_; }

 private:
  const Application& app_;
  VkRenderPass render_pass_;
  std::vector<VkFramebuffer> framebuffers_;
};

} /* namespace vulkan */

#endif /* LEARNVULKAN_RENDER_PASS_H */
