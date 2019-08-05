//
//  window_context.h
//
//  Created by Pujun Lun on 6/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_WINDOW_CONTEXT_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_WINDOW_CONTEXT_H

#include <string>

#include "jessie_steamer/common/window.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/swapchain.h"
#include "third_party/glm/glm.hpp"
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
class WindowContext {
 public:
  WindowContext() : context_{BasicContext::GetContext()},
                    surface_{context_} {}

  // This class is neither copyable nor movable.
  WindowContext(const WindowContext&) = delete;
  WindowContext& operator=(const WindowContext&) = delete;

  void Init(const std::string& name, int width = 800, int height = 600,
            absl::optional<MultisampleImage::Mode> multisampling_mode =
                MultisampleImage::Mode::kEfficient,
            const VkAllocationCallbacks* allocator = nullptr) {
    if (is_first_time_) {
      is_first_time_ = false;
      window_.Init(name, {width, height});
      auto create_surface = [this](const VkAllocationCallbacks* allocator,
                                   const VkInstance& instance) {
        *surface_ = window_.CreateSurface(instance, allocator);
      };
      context_->Init(allocator, WindowSupport{
          &*surface_,
          common::Window::GetRequiredExtensions(),
          Swapchain::required_extensions(),
          create_surface,
      });
    }
    auto screen_size = window_.GetScreenSize();
    swapchain_.Init(context_, *surface_,
                    {static_cast<uint32_t>(screen_size.x),
                     static_cast<uint32_t>(screen_size.y)},
                    multisampling_mode);
  }

  // Checks events and returns whether the window should continue to show.
  // Callbacks set via window will be invoked if triggering events are detected.
  bool CheckEvents() {
    window_.ProcessUserInputs();
    return !window_.ShouldQuit();
  }

  void Cleanup() {
    context_->WaitIdle();
    swapchain_.Cleanup();
    window_.Recreate();
  }

  // Accessors.
  SharedBasicContext basic_context() const { return context_; }
  common::Window& window() { return window_; }
  const VkSwapchainKHR& swapchain() const { return *swapchain_; }
  VkExtent2D frame_size() const { return swapchain_.image_extent(); }
  int num_swapchain_image() const {
    return swapchain_.num_swapcahin_image();
  }
  const Image& swapchain_image(int index) const {
    return swapchain_.swapcahin_image(index);
  }
  const Image& multisample_image() const {
    return swapchain_.multisample_image();
  }

 private:
  class Surface {
   public:
    explicit Surface(SharedBasicContext context)
        : context_{std::move(context)} {}

    // This class is neither copyable nor movable
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    ~Surface() {
      vkDestroySurfaceKHR(*context_->instance(), surface_,
                          context_->allocator());
    }

    VkSurfaceKHR& operator*() { return surface_; }

   private:
    SharedBasicContext context_;
    VkSurfaceKHR surface_;
  };

  bool is_first_time_ = true;
  SharedBasicContext context_;
  common::Window window_;
  Surface surface_;
  Swapchain swapchain_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_WINDOW_CONTEXT_H */
