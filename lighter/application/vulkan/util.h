//
//  util.h
//
//  Created by Pujun Lun on 6/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_APPLICATION_VULKAN_UTIL_H
#define LIGHTER_APPLICATION_VULKAN_UTIL_H

#include <cstdlib>
#include <memory>
#include <string_view>
#include <type_traits>

#include "lighter/application/util.h"
#include "lighter/common/camera.h"
#include "lighter/common/file.h"
#include "lighter/common/image.h"
#include "lighter/common/timer.h"
#include "lighter/common/util.h"
#include "lighter/renderer/align.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/vulkan/extension/compute_pass.h"
#include "lighter/renderer/vulkan/extension/graphics_pass.h"
#include "lighter/renderer/vulkan/extension/model.h"
#include "lighter/renderer/vulkan/extension/naive_render_pass.h"
#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/buffer.h"
#include "lighter/renderer/vulkan/wrapper/command.h"
#include "lighter/renderer/vulkan/wrapper/image.h"
#include "lighter/renderer/vulkan/wrapper/pipeline.h"
#include "lighter/renderer/vulkan/wrapper/pipeline_util.h"
#include "lighter/renderer/vulkan/wrapper/render_pass.h"
#include "lighter/renderer/vulkan/wrapper/window_context.h"
#include "third_party/absl/flags/declare.h"
#include "third_party/absl/flags/flag.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"
#include "third_party/glm/gtc/matrix_transform.hpp"
#include "third_party/vulkan/vulkan.h"

ABSL_DECLARE_FLAG(bool, performance_mode);

namespace lighter {
namespace application {
namespace vulkan {

// This is the base class of all applications. Its constructor simply forwards
// all arguments to the constructor of WindowContext. Each application should
// overwrite MainLoop() to render custom scenes.
class Application {
 public:
  // This class is neither copyable nor movable.
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  ~Application() = default;

  // Main loop of the application.
  virtual void MainLoop() = 0;

 protected:
  template <typename... Args>
  explicit Application(Args&&... args)
      : window_context_{std::forward<Args>(args)...} {}

  // Accessors.
  const renderer::vulkan::WindowContext& window_context() {
    return window_context_;
  }
  renderer::vulkan::WindowContext* mutable_window_context() {
    return &window_context_;
  }
  renderer::vulkan::SharedBasicContext context() const {
    return window_context_.basic_context();
  }

 private:
  // Onscreen rendering context.
  renderer::vulkan::WindowContext window_context_;
};

// Holds identifiers of an attachment image.
class AttachmentInfo {
 public:
  explicit AttachmentInfo(std::string_view image_name)
      : image_name_{image_name} {}

  // Makes 'image_usage_tracker' track the usage of this image. The initial
  // usage of 'sample_image' is used as the current usage.
  void AddToTracker(renderer::ImageUsageTracker& image_usage_tracker,
                    const renderer::vulkan::Image& sample_image) {
    image_usage_tracker.TrackImage(image_name_, sample_image.GetInitialUsage());
  }

  // Creates an AttachmentConfig to be used in NaiveRenderPass.
  renderer::vulkan::NaiveRenderPass::AttachmentConfig MakeAttachmentConfig() {
    return {image_name_, &attachment_index_};
  }

  // Accessors.
  int index() const {
    ASSERT_HAS_VALUE(attachment_index_,
                     "Attachment has not been added to graphics pass");
    return attachment_index_.value();
  }

 private:
  // Image name. This is used to identify an image in GraphicsPass and
  // ImageUsageTracker.
  const std::string image_name_;

  // Attachment index. This is used to identify an image within a
  // VkAttachmentDescription array when constructing render passes.
  std::optional<int> attachment_index_;
};

// This class maintains a render pass internally. It assumes the color
// attachment that we are rendering to is backed by the swapchain image. Whether
// multisampling is used depends on whether it is turned on for
// 'window_context'. If the subpass config passed in by the user indicates that
// the depth stencil attachment is used in any subpass, this class will also
// create a depth stencil image internally.
class OnScreenRenderPassManager {
 public:
  explicit OnScreenRenderPassManager(
      const renderer::vulkan::WindowContext* window_context,
      const renderer::vulkan::NaiveRenderPass::SubpassConfig& subpass_config)
      : window_context_{*FATAL_IF_NULL(window_context)},
        subpass_config_{subpass_config} {}

  // This class is neither copyable nor movable.
  OnScreenRenderPassManager(const OnScreenRenderPassManager&) = delete;
  OnScreenRenderPassManager& operator=(
      const OnScreenRenderPassManager&) = delete;

  // Recreates 'render_pass_'. If the depth stencil attachment is used in any
  // subpass, this will also recreate 'depth_stencil_image_' with the current
  // window framebuffer size. If this is called the first time, it will also
  // create 'render_pass_builder_' according to 'subpass_config_'.
  // This should be called once after the window is created, and whenever the
  // window is resized.
  void RecreateRenderPass();

  // Accessors.
  const renderer::vulkan::RenderPass& render_pass() const {
    return *render_pass_;
  }

 private:
  // Populates 'render_pass_builder_'.
  void CreateRenderPassBuilder();

  // Objects used for rendering.
  const renderer::vulkan::WindowContext& window_context_;
  const renderer::vulkan::NaiveRenderPass::SubpassConfig subpass_config_;
  AttachmentInfo swapchain_image_info_{"Swapchain"};
  AttachmentInfo multisample_image_info_{"Multisample"};
  AttachmentInfo depth_stencil_image_info_{"Depth stencil"};
  std::unique_ptr<renderer::vulkan::Image> depth_stencil_image_;
  std::unique_ptr<renderer::vulkan::RenderPassBuilder> render_pass_builder_;
  std::unique_ptr<renderer::vulkan::RenderPass> render_pass_;
};

// Parses command line arguments, sets necessary environment variables,
// instantiates an application of AppType, and runs its MainLoop().
// AppType must be a subclass of Application. 'app_args' will be forwarded to
// the constructor of the application.
template <typename AppType, typename... AppArgs>
int AppMain(int argc, char* argv[], AppArgs&&... app_args) {
  static_assert(std::is_base_of<Application, AppType>::value,
                "Not a subclass of Application");

  common::util::ParseCommandLine(argc, argv);
  common::file::EnableRunfileLookup(argv[0]);

#if defined(__APPLE__)
  if (absl::GetFlag(FLAGS_performance_mode)) {
    // To avoid the frame rate being clamped on MacOS when using MoltenVK:
    // https://github.com/KhronosGroup/MoltenVK/issues/581#issuecomment-487293665
    setenv("MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS", "0", /*overwrite=*/1);
    setenv("MVK_CONFIG_PRESENT_WITH_COMMAND_BUFFER", "0", /*overwrite=*/1);
  }
#endif /* __APPLE__ */

  // Set up the path to find Vulkan SDK.
  using common::file::GetVulkanSdkPath;

#if defined(__APPLE__)
  setenv("VK_ICD_FILENAMES",
         GetVulkanSdkPath("share/vulkan/icd.d/MoltenVK_icd.json").c_str(),
         /*overwrite=*/1);
#ifndef NDEBUG
  setenv("VK_LAYER_PATH",
         GetVulkanSdkPath("share/vulkan/explicit_layer.d").c_str(),
         /*overwrite=*/1);
#endif /* !NDEBUG */

#elif defined(__linux__)
#ifndef NDEBUG
  setenv("VK_LAYER_PATH",
         GetVulkanSdkPath("etc/vulkan/explicit_layer.d").c_str(),
         /*overwrite=*/1);
#endif /* !NDEBUG */

#endif /* __APPLE__ || __linux__ */

  // We don't catch exceptions in the debug mode, so that if there is anything
  // wrong, the debugger would stay at the point where the application breaks.
//#ifdef NDEBUG
  try {
//#endif /* NDEBUG */
    AppType app{std::forward<AppArgs>(app_args)...};
    app.MainLoop();
//#ifdef NDEBUG
  } catch (const std::exception& e) {
    LOG_ERROR << "Error: " << e.what();
    return EXIT_FAILURE;
  }
//#endif /* NDEBUG */

  return EXIT_SUCCESS;
}

} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */

#endif /* LIGHTER_APPLICATION_VULKAN_UTIL_H */
