//
//  renderer.cc
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/renderer.h"

#include <cstdlib>

#include "lighter/base/file.h"
#include "lighter/base/util.h"
#include "lighter/renderer/vk/util.h"

LIGHTER_FLAG(bool, ignore_vsync, false,
             "Ignore VSync and present images to the screen as fast as "
             "possible");

namespace lighter::renderer::vk {
namespace {

constexpr int kNumFramesInFlight = 2;

std::vector<const base::Window*> ToConstWindowPtrs(
    std::span<base::Window* const> windows) {
  return {windows.begin(), windows.end()};
}

}  // namespace

void InitVulkan() {
  using base::file::GetVulkanSdkPath;

  // TODO: Limit framerate on Windows. Present mode need to be changed as well.
#ifdef __APPLE__
  // TODO: Revisit this.
  if (FLAGS_ignore_vsync.Get()) {
    // To avoid the frame rate being clamped on MacOS when using MoltenVK:
    // https://github.com/KhronosGroup/MoltenVK/issues/581#issuecomment-487293665
    setenv("MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS", "0", /*overwrite=*/1);
    setenv("MVK_CONFIG_PRESENT_WITH_COMMAND_BUFFER", "0", /*overwrite=*/1);
  }

  if (std::getenv("VULKAN_SDK") != nullptr) {
    // export DYLD_LIBRARY_PATH=$VULKAN_SDK/lib:$DYLD_LIBRARY_PATH
    const std::string dyld_lib_path =
        GetVulkanSdkPath("lib") + ":$DYLD_LIBRARY_PATH";
    setenv("DYLD_LIBRARY_PATH", dyld_lib_path.c_str(), /*overwrite=*/1);

    // export VK_ICD_FILENAMES=$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json
    setenv("VK_ICD_FILENAMES",
           GetVulkanSdkPath("share/vulkan/icd.d/MoltenVK_icd.json").c_str(),
           /*overwrite=*/1);

    // export VK_LAYER_PATH=$VULKAN_SDK/share/vulkan/explicit_layer.d
    setenv("VK_LAYER_PATH",
           GetVulkanSdkPath("share/vulkan/explicit_layer.d").c_str(),
           /*overwrite=*/1);
  }
#endif  // __APPLE__

#ifdef __linux__
  if (std::getenv("VULKAN_SDK") != nullptr) {
    // export PATH=$VULKAN_SDK/bin:$PATH
    const std::string path = GetVulkanSdkPath("bin") + ":$PATH";
    setenv("PATH", path.c_str(), /*overwrite=*/1);

    // export LD_LIBRARY_PATH=$VULKAN_SDK/lib
    setenv("LD_LIBRARY_PATH", GetVulkanSdkPath("lib").c_str(), /*overwrite=*/1);

    // export VK_LAYER_PATH=$VULKAN_SDK/etc/vulkan/explicit_layer.d
    setenv("VK_LAYER_PATH",
           GetVulkanSdkPath("etc/vulkan/explicit_layer.d").c_str(),
           /*overwrite=*/1);
  }
#endif  // __linux__
}

Renderer::Renderer(
    const char* application_name,
    const std::optional<ir::debug_message::Config>& debug_message_config,
    std::vector<base::Window*>&& window_ptrs)
    : WithSharedContext{Context::CreateContext(application_name,
                                               debug_message_config,
                                               ToConstWindowPtrs(window_ptrs))},
      ir::Renderer{std::move(window_ptrs)} {
  swapchains_.resize(num_windows());
  per_frame_commands_.resize(num_windows());
  current_frames_.resize(num_windows(), 0);
  render_graph_executor_ =
      std::make_unique<RenderGraphExecutor>(context_, kNumFramesInFlight);
  for (int i = 0; i < num_windows(); ++i) {
    swapchains_[i] = std::make_unique<Swapchain>(context_, i, *windows().at(i));
    per_frame_commands_[i] =
        std::make_unique<PerFrameCommand>(context_, kNumFramesInFlight);
  }
}

void Renderer::RecreateSwapchain(int window_index) {
  context_->WaitIdle();
  base::Window& window = mutable_window(window_index);
  window.Recreate();
  swapchains_[window_index] =
      std::make_unique<Swapchain>(context_, window_index, window);
}

bool Renderer::RenderFrame(int window_index, ir::RenderGraph& graph) {
  const base::Window& window = *windows().at(window_index);
  const Swapchain& swapchain = *swapchains_.at(window_index);
  if (window.is_resized() ||
      window.GetFrameSize() != util::ToVec(swapchain.extent())) {
    RecreateSwapchain(window_index);
    return false;
  }

  std::vector<const Swapchain*> swapchain_ptrs(swapchains_.size(), nullptr);
  for (size_t i = 0; i < swapchains_.size(); ++i) {
    swapchain_ptrs[i] = swapchains_[i].get();
  }
  std::vector<uint32_t> acquired_indices(swapchains_.size(), 0);

  const int frame_index = current_frames_[window_index];
  const bool presented = per_frame_commands_[window_index]->Run(
      frame_index, swapchain,
      [&](intl::CommandBuffer cmd, uint32_t image_index) {
        acquired_indices[window_index] = image_index;
        render_graph_executor_->Execute(cmd, frame_index, graph, swapchain_ptrs,
                                        acquired_indices);
      });
  current_frames_[window_index] = (frame_index + 1) % kNumFramesInFlight;

  if (!presented) {
    RecreateSwapchain(window_index);
    return false;
  }
  return true;
}

void Renderer::ExecuteRenderGraph(ir::RenderGraph& graph) {
  context_->RunOneTimeGraphicsCommand([&](intl::CommandBuffer cmd) {
    render_graph_executor_->Execute(cmd, /*frame_index=*/0, graph,
                                    /*swapchains=*/{},
                                    /*acquired_swapchain_image_indices=*/{});
  });
}

}  // namespace lighter::renderer::vk
