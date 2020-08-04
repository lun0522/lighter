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
#include <type_traits>

#include "lighter/common/camera.h"
#include "lighter/common/file.h"
#include "lighter/common/timer.h"
#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/extension/align.h"
#include "lighter/renderer/vulkan/extension/attachment_info.h"
#include "lighter/renderer/vulkan/extension/compute_pass.h"
#include "lighter/renderer/vulkan/extension/graphics_pass.h"
#include "lighter/renderer/vulkan/extension/image_util.h"
#include "lighter/renderer/vulkan/extension/model.h"
#include "lighter/renderer/vulkan/extension/naive_render_pass.h"
#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/buffer.h"
#include "lighter/renderer/vulkan/wrapper/command.h"
#include "lighter/renderer/vulkan/wrapper/image.h"
#include "lighter/renderer/vulkan/wrapper/image_usage.h"
#include "lighter/renderer/vulkan/wrapper/pipeline.h"
#include "lighter/renderer/vulkan/wrapper/pipeline_util.h"
#include "lighter/renderer/vulkan/wrapper/render_pass.h"
#include "lighter/renderer/vulkan/wrapper/render_pass_util.h"
#include "lighter/renderer/vulkan/wrapper/window_context.h"
#include "third_party/absl/flags/flag.h"
#include "third_party/absl/memory/memory.h"
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
  template <typename... Args>
  explicit Application(Args&&... args)
      : window_context_{std::forward<Args>(args)...} {}

  // This class is neither copyable nor movable.
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  // Main loop of the application.
  virtual void MainLoop() = 0;

 protected:
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

// This is the base class of simple applications. It assumes that we are
// rendering to only one color attachment, which is backed by the swapchain
// image. Whether multisampling is used depends on whether it is turned on for
// WindowContext. If the user passes in subpass configs that specifies the depth
// stencil attachment is used in any subpass, this class will also create a
// depth stencil image internally.
class SimpleApp : public Application {
 public:
  // Inherits constructor.
  using Application::Application;

  // This class is neither copyable nor movable.
  SimpleApp(const SimpleApp&) = delete;
  SimpleApp& operator=(const SimpleApp&) = delete;

 protected:
  // Recreates 'render_pass_'. If the depth stencil attachment is used in any
  // subpass, this will also recreate 'depth_stencil_image_' with the current
  // window framebuffer size. If this is called the first time, it will also
  // create 'render_pass_builder_' according to 'subpass_config'.
  // This should be called once after the window is created, and whenever the
  // window is resized.
  void RecreateRenderPass(
      const renderer::vulkan::NaiveRenderPass::SubpassConfig& subpass_config);

  // Accessors.
  const renderer::vulkan::RenderPass& render_pass() const {
    return *render_pass_;
  }

 private:
  // Populates 'render_pass_builder_'.
  void CreateRenderPassBuilder(
      const renderer::vulkan::NaiveRenderPass::SubpassConfig& subpass_config);

  // Objects used for rendering.
  renderer::vulkan::AttachmentInfo swapchain_image_info_{"Swapchain"};
  renderer::vulkan::AttachmentInfo multisample_image_info_{"Multisample"};
  renderer::vulkan::AttachmentInfo depth_stencil_image_info_{"Depth stencil"};
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

  if (absl::GetFlag(FLAGS_performance_mode)) {
    // To avoid the frame rate being clamped on MacOS when using MoltenVK:
    // https://github.com/KhronosGroup/MoltenVK/issues/581#issuecomment-487293665
    setenv("MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS", "0", /*overwrite=*/1);
    setenv("MVK_CONFIG_PRESENT_WITH_COMMAND_BUFFER", "0", /*overwrite=*/1);
  }

  // Set up the path to find Vulkan SDK.
  using common::file::GetVulkanSdkPath;
  setenv("VK_ICD_FILENAMES",
         GetVulkanSdkPath("share/vulkan/icd.d/MoltenVK_icd.json").c_str(),
         /*overwrite=*/1);
#ifndef NDEBUG
  setenv("VK_LAYER_PATH",
         GetVulkanSdkPath("share/vulkan/explicit_layer.d").c_str(),
         /*overwrite=*/1);
#endif /* !NDEBUG */

  // We don't catch exceptions in the debug mode, so that if there is anything
  // wrong, the debugger would stay at the point where the application breaks.
#ifdef NDEBUG
  try {
#endif /* NDEBUG */
    AppType app{std::forward<AppArgs>(app_args)...};
    app.MainLoop();
#ifdef NDEBUG
  } catch (const std::exception& e) {
    LOG_ERROR << "Error: " << e.what();
    return EXIT_FAILURE;
  }
#endif /* NDEBUG */

  return EXIT_SUCCESS;
}

// TODO: Move this class to another file?
// This class is used for rendering the given image to full screen. It is mainly
// used for debugging.
class ImageViewer {
 public:
  ImageViewer(const renderer::vulkan::SharedBasicContext& context,
              const renderer::vulkan::SamplableImage& image,
              int num_channels, bool flip_y);

  // This class is neither copyable nor movable.
  ImageViewer(const ImageViewer&) = delete;
  ImageViewer& operator=(const ImageViewer&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  void UpdateFramebuffer(const VkExtent2D& frame_size,
                         const renderer::vulkan::RenderPass& render_pass,
                         uint32_t subpass_index);

  // Renders the image.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer) const;

 private:
  // Objects used for rendering.
  std::unique_ptr<renderer::vulkan::StaticDescriptor> descriptor_;
  std::unique_ptr<renderer::vulkan::PerVertexBuffer> vertex_buffer_;
  std::unique_ptr<renderer::vulkan::GraphicsPipelineBuilder> pipeline_builder_;
  std::unique_ptr<renderer::vulkan::Pipeline> pipeline_;
};

} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */

#endif /* LIGHTER_APPLICATION_VULKAN_UTIL_H */
