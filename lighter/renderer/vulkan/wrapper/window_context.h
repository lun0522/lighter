//
//  window_context.h
//
//  Created by Pujun Lun on 6/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_WRAPPER_WINDOW_CONTEXT_H
#define LIGHTER_RENDERER_VULKAN_WRAPPER_WINDOW_CONTEXT_H

#include <memory>
#include <optional>
#include <string>

#include "lighter/common/window.h"
#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/image.h"
#include "lighter/renderer/vulkan/wrapper/swapchain.h"
#ifndef NDEBUG
#include "lighter/renderer/vulkan/wrapper/validation.h"
#endif /* !NDEBUG */
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {

// Members of this class are required for onscreen rendering.
class WindowContext {
 public:
  // Configurations used to initialize the window context.
  struct Config {
    // Modifiers.
    Config& set_screen_size(int width, int height) {
      screen_size = {width, height};
      return *this;
    }

    Config& set_multisampling_mode(MultisampleImage::Mode mode) {
      multisampling_mode = mode;
      return *this;
    }

    Config& disable_multisampling() {
      multisampling_mode = std::nullopt;
      return *this;
    }

#ifndef NDEBUG
    Config& set_debug_callback_trigger(
        const DebugCallback::TriggerCondition& trigger) {
      debug_callback_trigger = trigger;
      return *this;
    }
#endif /* !NDEBUG */

    glm::ivec2 screen_size{800, 600};
    std::optional<MultisampleImage::Mode> multisampling_mode =
        MultisampleImage::Mode::kEfficient;
#ifndef NDEBUG
    DebugCallback::TriggerCondition debug_callback_trigger;
#endif /* !NDEBUG */
  };

  WindowContext(const std::string& name, const Config& config)
    : window_{name, config.screen_size},
      multisampling_mode_{config.multisampling_mode} {
    const WindowSupport window_support{
        common::Window::GetRequiredExtensions(),
        Swapchain::GetRequiredExtensions(),
        *surface_,
        [this](const BasicContext* context) {
          surface_.Init(context, window_.CreateSurface(*context->instance(),
                                                       *context->allocator()));
        },
    };
    context_ =
#ifdef NDEBUG
        BasicContext::GetContext(window_support);
#else  /* !NDEBUG */
        BasicContext::GetContext(window_support, config.debug_callback_trigger);
#endif /* NDEBUG */
    CreateSwapchain(window_.GetFrameSize());
  }

  // This class is neither copyable nor movable.
  WindowContext(const WindowContext&) = delete;
  WindowContext& operator=(const WindowContext&) = delete;

  // Returns whether the window context needs to be recreated.
  bool ShouldRecreate() const { return window_.is_resized(); }

  // Waits for the graphics device idle and the window finishes resizing, and
  // recreates expired resource. This should be called before other recreations.
  void Recreate() {
    context_->WaitIdle();
    const glm::ivec2 frame_size = window_.Recreate();
    CreateSwapchain(frame_size);
  }

  // Checks events and returns whether the window should continue to show.
  // Callbacks set via window will be invoked if triggering events are detected.
  bool CheckEvents() {
    window_.ProcessUserInputs();
    return !window_.ShouldQuit();
  }

  // Bridges to BasicContext::OnExit(). This should be called when the program
  // is about to end, and right before other resources get destroyed.
  void OnExit() { context_->OnExit(); }

  // Accessors.
  SharedBasicContext basic_context() const { return context_; }
  common::Window* mutable_window() { return &window_; }
  const common::Window& window() const { return window_; }
  float original_aspect_ratio() const {
    return window_.original_aspect_ratio();
  }
  const VkSwapchainKHR& swapchain() const { return **swapchain_; }
  const VkExtent2D& frame_size() const { return swapchain_->image_extent(); }
  int num_swapchain_images() const { return swapchain_->num_images(); }
  const Image& swapchain_image(int index) const {
    return swapchain_->image(index);
  }
  bool use_multisampling() const { return swapchain_->use_multisampling(); }
  VkSampleCountFlagBits sample_count() const {
    return swapchain_->sample_count();
  }
  std::optional<MultisampleImage::Mode> multisampling_mode() const {
    return multisampling_mode_;
  }
  // The user is responsible for checking if multisampling is used.
  const Image& multisample_image() const {
    return swapchain_->multisample_image();
  }

 private:
  // Creates a swapchain with the given 'frame_size'. This must not be called
  // before 'context_' and 'surface_' are created.
  void CreateSwapchain(const glm::ivec2& frame_size) {
    swapchain_ = std::make_unique<Swapchain>(
        context_, surface_,
        VkExtent2D{
            static_cast<uint32_t>(frame_size.x),
            static_cast<uint32_t>(frame_size.y),
        },
        multisampling_mode_);
  }

  // Pointer to basic context.
  SharedBasicContext context_;

  // Wrapper of GLFWwindow.
  common::Window window_;

  // Multisampling mode for swapchain images.
  const std::optional<MultisampleImage::Mode> multisampling_mode_;

  // Wrapper of VkSurfaceKHR.
  Surface surface_;

  // Wrapper of VkSwapchainKHR.
  std::unique_ptr<Swapchain> swapchain_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_WRAPPER_WINDOW_CONTEXT_H */
