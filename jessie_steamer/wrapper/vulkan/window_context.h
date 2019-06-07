//
//  window_context.h
//
//  Created by Pujun Lun on 6/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_WINDOW_CONTEXT_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_WINDOW_CONTEXT_H

#include <string>
#include <type_traits>

#include "jessie_steamer/common/window.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/swapchain.h"
#include "third_party/vulkan/vulkan.h"

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
template <typename WindowClass>
class WindowContext {
 public:
  WindowContext() : context_{BasicContext::GetContext()} {
    static_assert(std::is_base_of<common::Window, WindowClass>::value,
                  "Not a subclass of Window");
  }

  ~WindowContext() {
    vkDestroySurfaceKHR(*context_->instance(), surface_, context_->allocator());
  }

  void Init(const std::string& name, int width, int height,
            const VkAllocationCallbacks* allocator) {
    if (is_first_time_) {
      is_first_time_ = false;
      window_.Init(name, {width, height});
      auto create_surface = [this](const VkAllocationCallbacks* allocator,
                                   const VkInstance& instance) {
        // TODO: possible to put this directly in BasicContext?
        window_.CreateSurface(instance, allocator);
      };
      context_->Init(allocator, /*window_support=*/{
          /*is_required=*/true,
          &surface_,
          common::GetExtensionsRequiredForWindow<WindowClass>(),
          Swapchain::required_extensions(),
          create_surface,
      });
    }
    auto screen_size = window_.screen_size();
    swapchain_.Init(context_, surface_, screen_size.x, screen_size.y);
  }

  void Cleanup() { swapchain_.Cleanup(); }

  // This class is neither copyable nor movable.
  WindowContext(const WindowContext&) = delete;
  WindowContext& operator=(const WindowContext&) = delete;

  const WindowClass& window()   const { return window_; }
  const Swapchain& swapchain()  const { return swapchain_; }

 private:
  bool is_first_time_ = true;
  SharedBasicContext context_;
  WindowClass window_;
  Swapchain swapchain_;
  VkSurfaceKHR surface_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_WINDOW_CONTEXT_H */
