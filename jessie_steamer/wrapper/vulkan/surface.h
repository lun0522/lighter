//
//  surface.h
//
//  Created by Pujun Lun on 6/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_SURFACE_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_SURFACE_H

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

/** VkSurfaceKHR interfaces with platform-specific window systems. It is backed
 *    by the window created by GLFW, which hides platform-specific details.
 *    It is not needed for off-screen rendering.
 *
 *  Initialization (by GLFW):
 *    VkInstance
 *    GLFWwindow
 */
class Surface {
 public:
  Surface() = default;

  // This class is neither copyable nor movable
  Surface(const Surface&) = delete;
  Surface& operator=(const Surface&) = delete;

  ~Surface();

  void Init(std::shared_ptr<Context> context);

  const VkSurfaceKHR& operator*() const { return surface_; }

 private:
  std::shared_ptr<Context> context_;
  VkSurfaceKHR surface_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_SURFACE_H */
