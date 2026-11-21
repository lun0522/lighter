//
//  renderer.cc
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/renderer.h"

#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk {
namespace {

constexpr int kNumFramesInFlight = 2;

std::vector<const common::Window*> ToConstWindowPtrs(
    std::span<common::Window* const> windows) {
  return {windows.begin(), windows.end()};
}

}  // namespace

Renderer::Renderer(
    const char* application_name,
    const std::optional<ir::debug_message::Config>& debug_message_config,
    std::vector<common::Window*>&& window_ptrs)
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
  common::Window& window = mutable_window(window_index);
  window.Recreate();
  swapchains_[window_index] =
      std::make_unique<Swapchain>(context_, window_index, window);
}

bool Renderer::RenderFrame(int window_index, ir::RenderGraph& graph) {
  const common::Window& window = *windows().at(window_index);
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
