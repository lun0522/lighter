//
//  window_context.h
//
//  Created by Pujun Lun on 6/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_WINDOW_CONTEXT_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_WINDOW_CONTEXT_H

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "jessie_steamer/common/window.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/swapchain.h"
#ifndef NDEBUG
#include "jessie_steamer/wrapper/vulkan/validation.h"
#endif /* !NDEBUG */
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// Members of this class are required for on-screen rendering.
class WindowContext {
 public:
  // Configurations used to initialize the window context.
  // Swapchain images will use multisampling unless 'multisampling_mode' is set
  // to absl::nullopt,
  struct Config {
    glm::ivec2 screen_size{800, 600};
    absl::optional<MultisampleImage::Mode> multisampling_mode =
        MultisampleImage::Mode::kEfficient;
#ifndef NDEBUG
    DebugCallback::TriggerCondition debug_callback_trigger;
#endif /* !NDEBUG */
  };

  WindowContext(const std::string& name, const Config& config)
    : window_{name, config.screen_size},
      multisampling_mode_{config.multisampling_mode} {
    const WindowSupport window_support{
        &surface_,
        common::Window::GetRequiredExtensions(),
        Swapchain::GetRequiredExtensions(),
        [this](const VkInstance& instance,
               const VkAllocationCallbacks* allocator) {
          surface_ = window_.CreateSurface(instance, allocator);
        },
    };
    context_ =
#ifdef NDEBUG
        BasicContext::GetContext(window_support);
#else  /* !NDEBUG */
        BasicContext::GetContext(window_support, config.debug_callback_trigger);
#endif /* NDEBUG */
    CreateSwapchain(window_.GetScreenSize());
  }

  // This class is neither copyable nor movable.
  WindowContext(const WindowContext&) = delete;
  WindowContext& operator=(const WindowContext&) = delete;

  ~WindowContext() {
    // Explicitly destroy swapchain before surface as required by Vulkan.
    swapchain_ = nullptr;
    vkDestroySurfaceKHR(*context_->instance(), surface_,
                        *context_->allocator());
  }

  // Returns whether the window context needs to be recreated.
  bool ShouldRecreate() const { return window_.is_resized(); }

  // Waits for the graphics device idle and the window finishes resizing, and
  // recreates expired resource. This should be called before other recreations.
  void Recreate() {
    context_->WaitIdle();
    const glm::ivec2 screen_size = window_.Recreate();
    CreateSwapchain(screen_size);
  }

  // Checks events and returns whether the window should continue to show.
  // Callbacks set via window will be invoked if triggering events are detected.
  bool CheckEvents() {
    window_.ProcessUserInputs();
    return !window_.ShouldQuit();
  }

  // Accessors.
  SharedBasicContext basic_context() const { return context_; }
  common::Window& window() { return window_; }
  const VkSwapchainKHR& swapchain() const { return **swapchain_; }
  const VkExtent2D& frame_size() const { return swapchain_->image_extent(); }
  int num_swapchain_image() const {
    return swapchain_->num_images();
  }
  const Image& swapchain_image(int index) const {
    return swapchain_->image(index);
  }
  const Image& multisample_image() const {
    return swapchain_->multisample_image();
  }
  const absl::optional<MultisampleImage::Mode> multisampling_mode() const {
    return multisampling_mode_;
  }

 private:
  // Creates a swapchain with the given 'screen_size'. This must not be called
  // before 'context_' and 'surface_' are created.
  void CreateSwapchain(const glm::ivec2& screen_size) {
    swapchain_ = absl::make_unique<Swapchain>(
        context_, surface_,
        VkExtent2D{
            static_cast<uint32_t>(screen_size.x),
            static_cast<uint32_t>(screen_size.y),
        },
        multisampling_mode_);
  }

  // Pointer to basic context.
  SharedBasicContext context_;

  // Wrapper of GLFWwindow.
  common::Window window_;

  // Multisampling mode for swapchain images.
  const absl::optional<MultisampleImage::Mode> multisampling_mode_;

  // VkSurfaceKHR interfaces with platform-specific window systems.
  VkSurfaceKHR surface_;

  // Wrapper of VkSwapchainKHR.
  std::unique_ptr<Swapchain> swapchain_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_WINDOW_CONTEXT_H */
