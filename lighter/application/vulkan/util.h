//
//  util.h
//
//  Created by Pujun Lun on 6/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_APPLICATION_VULKAN_UTIL_H
#define LIGHTER_APPLICATION_VULKAN_UTIL_H

#include <cstdlib>
#include <functional>
#include <type_traits>

#include "lighter/common/camera.h"
#include "lighter/common/file.h"
#include "lighter/common/timer.h"
#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/extension/align.h"
#include "lighter/renderer/vulkan/extension/compute_pass.h"
#include "lighter/renderer/vulkan/extension/graphics_pass.h"
#include "lighter/renderer/vulkan/extension/image_util.h"
#include "lighter/renderer/vulkan/extension/model.h"
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
#include "third_party/absl/types/optional.h"
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

// TODO: Move this class to another file?
// Holds identifiers of an attachment image and provides util functions for
// interacting with image::UsageTracker and GraphicsPass.
class AttachmentInfo {
 public:
  explicit AttachmentInfo(std::string&& name) : name_{std::move(name)} {}

  // This class provides copy constructor and move constructor.
  AttachmentInfo(AttachmentInfo&&) noexcept = default;
  AttachmentInfo(AttachmentInfo&) = default;

  // Makes 'image_usage_tracker' track the usage of this image.
  AttachmentInfo& AddToTracker(
      renderer::vulkan::image::UsageTracker& image_usage_tracker,
      const renderer::vulkan::Image& sample_image) {
    image_usage_tracker.TrackImage(name_, sample_image);
    return *this;
  }

  // Performs following steps:
  // (1) Retrieve the initial usage from 'image_usage_tracker' and use it to
  //     construct a usage history.
  // (2) Call 'populate_history' to populate subpasses of the history.
  // (3) Add the history to 'graphics_pass' and records the attachment index.
  // (4) Update 'image_usage_tracker' to track the last usage of this image in
  //     'graphics_pass'.
  AttachmentInfo& AddToGraphicsPass(
      renderer::vulkan::GraphicsPass& graphics_pass,
      renderer::vulkan::image::UsageTracker& image_usage_tracker,
      const std::function<void(renderer::vulkan::image::UsageHistory&)>&
          populate_history);

  // Informs 'graphics_pass' that this attachment will resolve to
  // 'target_attachment' at 'subpass'.
  AttachmentInfo& ResolveToAttachment(
      renderer::vulkan::GraphicsPass& graphics_pass,
      const AttachmentInfo& target_attachment, int subpass) {
    graphics_pass.AddMultisampleResolving(name_, target_attachment.name_,
                                          subpass);
    return *this;
  }

  // Accessors.
  int index() const { return index_.value(); }

 private:
  // Image name. This is used to identify an image in classes like ComputePass,
  // GraphicsPass and image::UsageTracker.
  std::string name_;

  // Attachment index. This is used to identify an image within a
  // VkAttachmentDescription array when constructing render passes.
  absl::optional<int> index_;
};

} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */

#endif /* LIGHTER_APPLICATION_VULKAN_UTIL_H */
